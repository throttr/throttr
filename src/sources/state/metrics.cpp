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

#include <boost/asio/bind_executor.hpp>

#include <fmt/chrono.h>

namespace throttr
{
#ifdef ENABLED_FEATURE_METRICS
  void state::process_metrics()
  {
#ifndef NDEBUG
    fmt::println("{:%Y-%m-%d %H:%M:%S} METRICS SNAPSHOT STARTED", std::chrono::system_clock::now());
#endif
    for (auto &_index = storage_.get<tag_by_key>(); auto &_entry : _index) // LCOV_EXCL_LINE Note: Partially tested.
    {
      // LCOV_EXCL_START
      if (_entry.expired_)
        continue;
      // LCOV_EXCL_STOP

      auto
        &[stats_reads_,
          stats_writes_,
          stats_reads_accumulator_,
          stats_writes_accumulator_,
          stats_reads_per_minute_,
          stats_writes_per_minute_] = *_entry.metrics_;
      const auto reads = stats_reads_.exchange(0, std::memory_order_relaxed);
      const auto writes = stats_writes_.exchange(0, std::memory_order_relaxed);

      stats_reads_per_minute_.store(reads, std::memory_order_relaxed);
      stats_writes_per_minute_.store(writes, std::memory_order_relaxed);

      stats_reads_accumulator_.fetch_add(reads, std::memory_order_relaxed);
      stats_writes_accumulator_.fetch_add(writes, std::memory_order_relaxed);
    }

#ifndef NDEBUG
    fmt::println("{:%Y-%m-%d %H:%M:%S} METRICS SNAPSHOT COMPLETED", std::chrono::system_clock::now());
#endif
  }

  void state::start_metrics_timer()
  {
#ifndef NDEBUG
    fmt::println("{:%Y-%m-%d %H:%M:%S} METRICS SNAPSHOT SCHEDULED", std::chrono::system_clock::now());
#endif

    metrics_timer_.expires_after(std::chrono::minutes(1));
    metrics_timer_.async_wait(boost::asio::bind_executor(
      strand_,
      [_self = shared_from_this()](const boost::system::error_code &ec)
      {
        if (!ec)
        {
          _self->process_metrics();
          _self->start_metrics_timer(); // reschedule
        }
      }));
  }
#endif
} // namespace throttr