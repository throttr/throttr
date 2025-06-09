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

    const auto &_subs = state->subscriptions_->subscriptions_.get<by_channel_name>();
    const auto _channel =
      std::string_view(reinterpret_cast<const char *>(_request.channel_.data()), _request.channel_.size()); // NOSONAR
    auto _range = _subs.equal_range(std::string(_channel));

    if (_range.first == _range.second) // LCOV_EXCL_LINE Note: Partially tested.
    {
      // LCOV_EXCL_START
#ifndef NDEBUG
      fmt::println(
        "{:%Y-%m-%d %H:%M:%S} REQUEST CHANNEL session_id={} META channel={} RESPONSE ok=false",
        std::chrono::system_clock::now(),
        to_string(id),
        span_to_hex(_request.channel_));
#endif
      // LCOV_EXCL_STOP
      batch.emplace_back(&state::failed_response_, 1);
      return;
    }

    batch.emplace_back(&state::success_response_, 1);

    {
      const auto _offset = write_buffer.size();
      const uint64_t _count = std::distance(_range.first, _range.second);
      append_uint64_t(write_buffer, native_to_little(_count));
      batch.emplace_back(&write_buffer[_offset], sizeof(_count));
    }

    for (auto it = _range.first; it != _range.second; ++it) // LCOV_EXCL_LINE Note: Partially tested.
    {
      const auto &_sub = *it;

      append_uuid(write_buffer, batch, _sub.connection_id_.data());

      {
        const auto _offset = write_buffer.size();
        append_uint64_t(write_buffer, native_to_little(_sub.subscribed_at_));
        batch.emplace_back(&write_buffer[_offset], sizeof(_sub.subscribed_at_));
      }

#ifdef ENABLED_FEATURE_METRICS
      const uint64_t _read = _sub.metrics_->read_bytes_.accumulator_.load(std::memory_order_relaxed);
      const uint64_t _write = _sub.metrics_->write_bytes_.accumulator_.load(std::memory_order_relaxed);
#else
      constexpr uint64_t _read = 0;
      constexpr uint64_t _write = 0;
#endif

      for (const uint64_t metric : {_read, _write})
      {
        const auto _offset = write_buffer.size();
        append_uint64_t(write_buffer, native_to_little(metric));
        batch.emplace_back(&write_buffer[_offset], sizeof(metric));
      }
    }

    // LCOV_EXCL_START
#ifndef NDEBUG
    fmt::println(
      "{:%Y-%m-%d %H:%M:%S} REQUEST CHANNEL session_id={} META channel={} RESPONSE ok=true",
      std::chrono::system_clock::now(),
      to_string(id),
      span_to_hex(_request.channel_));
#endif
    // LCOV_EXCL_STOP
  }
} // namespace throttr
