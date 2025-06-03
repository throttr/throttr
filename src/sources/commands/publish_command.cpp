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
#include <boost/uuid/uuid_io.hpp>
#include <throttr/connection.hpp>
#include <throttr/services/response_builder_service.hpp>
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
    const std::shared_ptr<connection> &conn)
  {
    boost::ignore_unused(type, batch, write_buffer);

    const auto _request = request_publish::from_buffer(view);
    const auto &_payload = _request.value_;

    const auto &_subs = state->subscriptions_->subscriptions_.get<by_channel_name>();
    const std::string _channel{
      std::string_view(reinterpret_cast<const char *>(_request.channel_.data()), _request.channel_.size())}; // NOSONAR
    const auto _range = _subs.equal_range(_channel);                                                         // NOSONAR

    // LCOV_EXCL_START Note: This means that there are no suscriptions.
    if (_range.first == _range.second)
      return;
    // LCOV_EXCL_STOP

    const auto _message = std::make_shared<message>();
    _message->recyclable_ = false;
    auto &_buffer = _message->write_buffer_;
    _buffer.reserve(1 + sizeof(value_type) + _payload.size());

    _buffer.push_back(std::byte{0x03});

    const auto _size = _payload.size();
    for (std::size_t _i = 0; _i < sizeof(value_type); ++_i)
    {
      _buffer.push_back(std::byte{static_cast<unsigned char>(_size >> (8 * _i) & 0xFF)});
    }

    for (const auto &_byte : _payload)
      _buffer.push_back(std::byte{std::to_integer<uint8_t>(_byte)});

    _message->buffers_.emplace_back(boost::asio::buffer(_buffer));

#ifdef ENABLED_FEATURE_METRICS
    conn->metrics_->network_.published_bytes_.mark(_payload.size());
#endif

    for (auto _it = _range.first; _it != _range.second; ++_it) // LCOV_EXCL_LINE Note: Partially tested.
    {
      const auto &_sub = *_it;
      const auto &_sub_id = _sub.connection_id_;

#ifdef ENABLED_FEATURE_METRICS
      const_cast<subscription &>(_sub).metrics_->read_bytes_.mark(_payload.size()); // NOSONAR
#endif

      if (_sub_id == conn->id_) // LCOV_EXCL_LINE Note: Partially tested.
      {
#ifdef ENABLED_FEATURE_METRICS
        const_cast<subscription &>(_sub).metrics_->write_bytes_.mark(_payload.size()); // NOSONAR
#endif
        continue;
      }

      const auto _conn_it = state->connections_.find(_sub_id);
      // LCOV_EXCL_START Note: This means that the subscribed connection was disconnected.
      if (_conn_it == state->connections_.end())
        continue;
        // LCOV_EXCL_STOP

#ifdef ENABLED_FEATURE_METRICS
      _conn_it->second->metrics_->network_.received_bytes_.mark(_payload.size());
#endif

      _conn_it->second->send(_message);
    }

    // LCOV_EXCL_START
#ifndef NDEBUG
    fmt::println(
      "{:%Y-%m-%d %H:%M:%S} REQUEST PUBLISH channel={} from={} data={} "
      "RESPONSE ok=true",
      std::chrono::system_clock::now(),
      _channel,
      to_string(conn->id_),
      span_to_hex(_payload));
#endif
    // LCOV_EXCL_STOP
  }
} // namespace throttr
