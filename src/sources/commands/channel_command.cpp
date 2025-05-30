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
#include <throttr/services/response_builder_service.hpp>
#include <throttr/state.hpp>

namespace throttr
{
  void channel_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::uint8_t> &write_buffer,
    const std::shared_ptr<connection> &conn)
  {

    boost::ignore_unused(type, conn);

#ifndef ENABLED_FEATURE_METRICS
    batch.emplace_back(boost::asio::buffer(&state::failed_response_, 1));
    return;
#endif

    const auto _request = request_channel::from_buffer(view);

    const auto &_subs = state->subscriptions_->subscriptions_.get<by_channel_name>();
    auto _range = _subs.equal_range(_request.channel_);

    if (_range.first == _range.second) // LCOV_EXCL_LINE Note: Partially tested.
    {
      batch.emplace_back(boost::asio::buffer(&state::failed_response_, 1));
      return;
    }

    batch.emplace_back(boost::asio::buffer(&state::success_response_, 1));

    const auto _offset = write_buffer.size();
    const uint64_t _count = std::distance(_range.first, _range.second);
    const auto *_count_ptr = reinterpret_cast<const std::uint8_t *>(&_count); // NOSONAR
    write_buffer.insert(write_buffer.end(), _count_ptr, _count_ptr + sizeof(_count));
    batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(_count)));

    for (auto it = _range.first; it != _range.second; ++it) // LCOV_EXCL_LINE Note: Partially tested.
    {
      const auto &_sub = *it;

      {
        const auto _off = write_buffer.size();
        write_buffer.resize(_off + 16);
        std::memcpy(write_buffer.data() + _off, _sub.connection_id_.data, 16); // NOSONAR
        batch.emplace_back(boost::asio::buffer(write_buffer.data() + _off, 16));
      }

      const auto *_ts_ptr = reinterpret_cast<const std::uint8_t *>(&_sub.subscribed_at_); // NOSONAR
      write_buffer.insert(write_buffer.end(), _ts_ptr, _ts_ptr + 8);
      batch.emplace_back(boost::asio::buffer(&write_buffer[write_buffer.size() - 8], 8));

      const uint64_t _read = _sub.metrics_.read_bytes_.load(std::memory_order_relaxed);
      const uint64_t _write = _sub.metrics_.write_bytes_.load(std::memory_order_relaxed);

      for (uint64_t metric : {_read, _write})
      {
        const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&metric); // NOSONAR
        write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(metric));
        batch.emplace_back(boost::asio::buffer(&write_buffer[write_buffer.size() - sizeof(metric)], sizeof(metric)));
      }
    }
  }
} // namespace throttr
