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

#include <throttr/state.hpp>

#include <fmt/chrono.h>

namespace throttr
{
  void state::handle_stat(
    const request_stat &request,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::uint8_t> &write_buffer)
  {
#ifndef ENABLED_FEATURE_METRICS
    batch.emplace_back(boost::asio::buffer(&failed_response_, 1));
    return;
#endif

    const request_key _key{request.key_};
    const auto _find = find_or_fail_for_batch(_key, batch);
    if (!_find.has_value()) // LCOV_EXCL_LINE
    {
      // LCOV_EXCL_START
#ifndef NDEBUG
      fmt::println("{:%Y-%m-%d %H:%M:%S} REQUEST STAT key={} RESPONSE ok=false", std::chrono::system_clock::now(), _key.key_);
#endif
      return;
      // LCOV_EXCL_STOP
    }
    const auto _it = _find.value();
    batch.emplace_back(boost::asio::buffer(&success_response_, 1));

    auto _append_uint64 = [&write_buffer, &batch](const uint64_t value)
    {
      const auto _offset = write_buffer.size();
      const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&value); // NOSONAR
      write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(uint64_t));
      batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(uint64_t)));
    };

    const auto &metrics = *_it->metrics_;
    _append_uint64(metrics.stats_reads_per_minute_.load(std::memory_order_relaxed));
    _append_uint64(metrics.stats_writes_per_minute_.load(std::memory_order_relaxed));
    _append_uint64(metrics.stats_reads_accumulator_.load(std::memory_order_relaxed));
    _append_uint64(metrics.stats_writes_accumulator_.load(std::memory_order_relaxed));

    // LCOV_EXCL_START
#ifndef NDEBUG
    fmt::println(
      "{:%Y-%m-%d %H:%M:%S} REQUEST STAT key={} RESPONSE ok=true read_per_minute={} write_per_minute={} read_total={} write_total={}",
      std::chrono::system_clock::now(),
      _key.key_,
      metrics.stats_reads_per_minute_.load(),
      metrics.stats_writes_per_minute_.load(),
      metrics.stats_reads_accumulator_.load(),
      metrics.stats_writes_accumulator_.load());
#endif
    // LCOV_EXCL_STOP
  }
} // namespace throttr