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

#ifndef THROTTR_ENTRY_METRICS_HPP
#define THROTTR_ENTRY_METRICS_HPP

#include <atomic>
#include <iostream>

namespace throttr
{
#ifdef ENABLED_FEATURE_METRICS
  /**
   * Entry metrics
   */
  struct entry_metrics
  {
    /**
     * Reads
     */
    std::atomic<uint64_t> reads_ = 0;

    /**
     * Writes
     */
    std::atomic<uint64_t> writes_ = 0;

    /**
     * Reads accumulator
     */
    std::atomic<uint64_t> reads_accumulator_ = 0;

    /**
     * Write accumulator
     */
    std::atomic<uint64_t> writes_accumulator_ = 0;

    /**
     * Reads per minute (RPM)
     */
    std::atomic<uint64_t> reads_per_minute_ = 0;

    /**
     * Writes per minute (WPM)
     */
    std::atomic<uint64_t> writes_per_minute_ = 0;
  };
#endif
} // namespace throttr

#endif // THROTTR_ENTRY_METRICS_HPP
