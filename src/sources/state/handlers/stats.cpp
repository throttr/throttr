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

namespace throttr
{
  void state::handle_stats(std::vector<boost::asio::const_buffer> &batch, std::vector<std::uint8_t> &write_buffer)
  {
#ifndef ENABLED_FEATURE_METRICS
    batch.emplace_back(boost::asio::buffer(&failed_response_, 1));
    return;
#endif

    handle_fragmented_response(
      batch,
      write_buffer,
      2048,
      [&write_buffer](std::vector<boost::asio::const_buffer> *b, const entry_wrapper *e, const bool measure)
      { return write_stats_entry_to_buffer(b, e, write_buffer, measure); });
  }

  std::size_t state::write_stats_entry_to_buffer(
    std::vector<boost::asio::const_buffer> *batch,
    const entry_wrapper *entry,
    std::vector<std::uint8_t> &write_buffer,
    const bool measure)
  {
    if (measure) // LCOV_EXCL_LINE Note: Partially tested.
      return entry->key_.size() + 1 + 8 * 4;

    const auto _offset = write_buffer.size();
    write_buffer.push_back(static_cast<std::uint8_t>(entry->key_.size()));
    batch->emplace_back(boost::asio::buffer(&write_buffer[_offset], 1));

    for (const auto &_metric = *entry->metrics_; uint64_t _v :
                                                 {_metric.reads_per_minute_.load(std::memory_order_relaxed),
                                                  _metric.writes_per_minute_.load(std::memory_order_relaxed),
                                                  _metric.reads_accumulator_.load(std::memory_order_relaxed),
                                                  _metric.writes_accumulator_.load(std::memory_order_relaxed)})
    {
      const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&_v); // NOSONAR
      const auto _off = write_buffer.size();
      write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(_v));
      batch->emplace_back(boost::asio::buffer(&write_buffer[_off], sizeof(_v)));
    }

    return 0;
  }
} // namespace throttr