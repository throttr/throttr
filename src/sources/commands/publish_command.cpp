// Copyright (C) 2025 Ian Torres
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include <throttr/commands/publish_command.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <throttr/connection.hpp>
#include <throttr/services/subscriptions_service.hpp>
#include <throttr/state.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  void publish_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer,
    const boost::uuids::uuid id)
  {
    boost::ignore_unused(type, batch, write_buffer);

    using namespace boost::endian;

    const auto _request = request_publish::from_buffer(view);
    const auto &_payload = _request.value_;

    const auto &_subs = state->subscriptions_->subscriptions_.get<by_channel_name>();
    const std::string _channel{
      std::string_view(reinterpret_cast<const char *>(_request.channel_.data()), _request.channel_.size())};
    const auto _range = _subs.equal_range(_channel);

    batch.reserve(batch.size() + 1);

    if (_range.first == _range.second)
    {
#ifndef NDEBUG
      fmt::println(
        "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST PUBLISH session_id={} META channel={} data={} RESPONSE ok=false",
        to_string(state->id_),
        std::chrono::system_clock::now(),
        to_string(id),
        _channel,
        span_to_hex(_payload));
#endif
      batch.emplace_back(&state::failed_response_, 1);
      return;
    }

    const auto _message = std::make_shared<message>();
    _message->recyclable_ = false;
    _message->buffers_.reserve(1);

    auto &_buffer = _message->write_buffer_;

    const auto _payload_size = _payload.size();
    const auto _total_size = sizeof(uint8_t) + sizeof(uint8_t) + sizeof(value_type) + _channel.size() + _payload_size;
    _buffer.resize(_total_size);

    std::size_t _offset = 0;

    // Type
    constexpr auto event_type = request_types::event;
    std::memcpy(_buffer.data() + _offset, &event_type, sizeof(uint8_t));
    _offset += sizeof(uint8_t);

    // Size
    const uint8_t _channel_size = static_cast<uint8_t>(_channel.size());
    std::memcpy(_buffer.data() + _offset, &_channel_size, sizeof(uint8_t));
    _offset += sizeof(uint8_t);

    // Size
    const value_type _size = native_to_little(static_cast<value_type>(_payload_size));
    std::memcpy(_buffer.data() + _offset, &_size, sizeof(value_type));
    _offset += sizeof(value_type);

    // Channel
    std::memcpy(_buffer.data() + _offset, _channel.data(), _channel_size);
    _offset += _channel_size;

    // Value
    std::memcpy(_buffer.data() + _offset, _payload.data(), _payload_size);

    _message->buffers_.emplace_back(_buffer.data(), _buffer.size());

    for (auto _it = _range.first; _it != _range.second; ++_it)
    {
      const auto &_sub = *_it;
      const auto &_sub_id = _sub.connection_id_;

#ifdef ENABLED_FEATURE_METRICS
      const_cast<subscription &>(_sub).metrics_->read_bytes_.mark(_payload.size());
#endif

      auto _is_tcp = false;
      auto _is_unix = false;
      auto _is_agent_tcp = false;
      auto _is_agent_unix = false;

      {
        std::scoped_lock _lock(state->tcp_connections_mutex_);
        _is_tcp = state->tcp_connections_.contains(_sub_id);
      }

      {
        std::scoped_lock _lock(state->unix_connections_mutex_);
        _is_unix = state->unix_connections_.contains(_sub_id);
      }

      {
        std::scoped_lock _lock(state->agent_unix_connections_mutex_);
        _is_agent_unix = state->agent_unix_connections_.contains(_sub_id);
      }

      {
        std::scoped_lock _lock(state->agent_tcp_connections_mutex_);
        _is_agent_tcp = state->agent_tcp_connections_.contains(_sub_id);
      }

      // This is a strange condition, the subscription exists but the connection is gone...
      if (!_is_tcp && !_is_unix && !_is_agent_tcp && !_is_agent_unix)
        continue;

      auto _process =
        [_scoped_message =
           _message
             ->shared_from_this()](auto &connections, auto &mutex, const auto &sub_id, const auto &scope_id, const auto &payload)
      {
        boost::ignore_unused(scope_id, payload);

        std::scoped_lock _lock(mutex);
        const auto _conn_it = connections.find(sub_id);
        if (_conn_it == connections.end())
          return;

        auto *_conn = _conn_it->second;

#ifdef ENABLED_FEATURE_METRICS
        if (_conn->id_ == scope_id)
          _conn->metrics_->network_.published_bytes_.mark(payload.size());
        else
          _conn->metrics_->network_.received_bytes_.mark(payload.size());
#endif

        _conn->send(_scoped_message);
      };

      if (_is_tcp)
        _process(state->tcp_connections_, state->tcp_connections_mutex_, _sub_id, id, _payload);

      if (_is_unix)
        _process(state->unix_connections_, state->unix_connections_mutex_, _sub_id, id, _payload);

      if (_is_agent_tcp)
        _process(state->agent_tcp_connections_, state->agent_tcp_connections_mutex_, _sub_id, id, _payload);

      if (_is_agent_unix)
        _process(state->agent_unix_connections_, state->agent_unix_connections_mutex_, _sub_id, id, _payload);
    }

#ifndef NDEBUG
    fmt::println(
      "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST PUBLISH session_id={} META channel={} data={} RESPONSE ok=true",
      to_string(state->id_),
      std::chrono::system_clock::now(),
      to_string(id),
      _channel,
      span_to_hex(_payload));
#endif

    batch.emplace_back(&state::success_response_, 1);
  }
} // namespace throttr
