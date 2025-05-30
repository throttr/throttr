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

#pragma once

#ifndef THROTTR_COMMAND_METRICS_HPP
#define THROTTR_COMMAND_METRICS_HPP

#include <atomic>

namespace throttr
{
#ifdef ENABLED_FEATURE_METRICS
  struct metric
  {
    /**
     * Temporal count
     */
    std::atomic<uint64_t> count_ = 0;

    /**
     * Historic accumulator
     */
    std::atomic<uint64_t> accumulator_ = 0;

    /**
     * Per minute
     */
    std::atomic<uint64_t> per_minute_ = 0;

    void mark(const std::size_t step = 1)
    {
      count_.fetch_add(step, std::memory_order_relaxed);
      accumulator_.fetch_add(step, std::memory_order_relaxed);
    }

    void compute()
    {
      per_minute_.store(count_.exchange(0, std::memory_order_relaxed), std::memory_order_relaxed);
    }
  };
#endif
} // namespace throttr

#endif // THROTTR_COMMAND_METRICS_HPP
