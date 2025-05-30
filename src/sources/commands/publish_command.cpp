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
#include <throttr/connection.hpp>
#include <throttr/services/response_builder_service.hpp>
#include <throttr/state.hpp>

namespace throttr
{
  void publish_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::uint8_t> &write_buffer,
    boost::uuids::uuid id)
  {
    boost::ignore_unused(type, batch, write_buffer);

    const auto _request = request_publish::from_buffer(view);
    const auto &_channel = _request.channel_;
    const auto &_payload = _request.value_;

    const auto &_subs = state->subscriptions_->subscriptions_.get<by_channel_name>();
    const auto _range = _subs.equal_range(_channel);

    if (_range.first == _range.second)
      return;

    const auto _message = std::make_shared<message>();
    auto &_buffer = _message->write_buffer_;
    _buffer.reserve(1 + sizeof(value_type) + _payload.size());

    _buffer.push_back(0x03);

    const value_type size = _request.header_->value_size_;
    const auto *size_bytes = reinterpret_cast<const uint8_t *>(&size);
    for (std::size_t _i = 0; _i < sizeof(value_type); ++_i)
      _buffer.push_back(size_bytes[_i]);

    for (const auto &_byte : _payload)
      _buffer.push_back(std::to_integer<uint8_t>(_byte));

    _message->buffers_.emplace_back(boost::asio::buffer(_buffer));

    for (auto _it = _range.first; _it != _range.second; ++_it)
    {
      const auto &_sub = *_it;
      if (_sub.connection_id_ == id)
        continue;

      const auto _conn_it = state->connections_.find(_sub.connection_id_);
      if (_conn_it == state->connections_.end())
        continue;

      _conn_it->second->send(_message);
    }
  }
} // namespace throttr
