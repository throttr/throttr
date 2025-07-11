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

#include <throttr/commands/channel_command.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <throttr/connection.hpp>
#include <throttr/services/subscriptions_service.hpp>
#include <throttr/state.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  void channel_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer,
    const boost::uuids::uuid id)
  {

    boost::ignore_unused(type, id);
    using namespace boost::endian;

    const auto _request = request_channel::from_buffer(view);

    std::size_t _offset = write_buffer.size();

    const auto &_subs = state->subscriptions_->subscriptions_.get<by_channel_name>();

    std::string _channel(_request.channel_.size(), '\0');
    std::memcpy(_channel.data(), _request.channel_.data(), _request.channel_.size());

    auto _range = _subs.equal_range(_channel);

    if (_range.first == _range.second)
    {
      batch.reserve(batch.size() + 1);

#ifndef NDEBUG
      fmt::println(
        "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST CHANNEL session_id={} META channel={} RESPONSE ok=false",
        to_string(state->id_),
        std::chrono::system_clock::now(),
        to_string(id),
        span_to_hex(_request.channel_));
#endif
      batch.emplace_back(&state::failed_response_, 1);
      return;
    }

    {
      const uint64_t _count = std::distance(_range.first, _range.second);
      const uint64_t _count_native = native_to_little(_count);

      // 1 status
      // 1 count
      // 4 * count in connection_id, subscribed_at, read and write
      batch.reserve(batch.size() + 1 + 1 + 4 * _count);

      // 8 bytes count
      // (16 + 8 * 3) * COUNT bytes in per connection id + subscribed at + read and write
      write_buffer.resize(write_buffer.size() + sizeof(uint64_t) + (16 + sizeof(uint64_t) * 3) * _count);

      batch.emplace_back(&state::success_response_, 1);

      std::memcpy(write_buffer.data() + _offset, &_count_native, sizeof(uint64_t));
      batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
      _offset += sizeof(uint64_t);
    }

    for (auto it = _range.first; it != _range.second; ++it)
    {
      const auto &_sub = *it;

      std::memcpy(write_buffer.data() + _offset, &_sub.connection_id_, 16);
      batch.emplace_back(write_buffer.data() + _offset, 16);
      _offset += 16;

      const auto _subscribed_at = native_to_little(_sub.subscribed_at_);
      std::memcpy(write_buffer.data() + _offset, &_subscribed_at, sizeof(uint64_t));
      batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
      _offset += sizeof(uint64_t);

#ifdef ENABLED_FEATURE_METRICS
      const uint64_t _read = native_to_little(_sub.metrics_->read_bytes_.accumulator_.load(std::memory_order_relaxed));
      const uint64_t _write = native_to_little(_sub.metrics_->write_bytes_.accumulator_.load(std::memory_order_relaxed));
#else
      constexpr uint64_t _read = 0;
      constexpr uint64_t _write = 0;
#endif

      for (const uint64_t metric : {_read, _write})
      {
        std::memcpy(write_buffer.data() + _offset, &metric, sizeof(uint64_t));
        batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
        _offset += sizeof(uint64_t);
      } // 16 bytes
    }

#ifndef NDEBUG
    fmt::println(
      "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST CHANNEL session_id={} META channel={} RESPONSE ok=true",
      to_string(state->id_),
      std::chrono::system_clock::now(),
      to_string(id),
      span_to_hex(_request.channel_));
#endif
  }
} // namespace throttr
