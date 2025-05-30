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

#ifndef THROTTR_SUBSCRIPTION_METRICS_HPP
#define THROTTR_SUBSCRIPTION_METRICS_HPP

#include <atomic>

namespace throttr
{
  struct subscription_metrics
  {
#ifdef ENABLED_FEATURE_METRICS
    /**
     * Bytes read
     */
    std::atomic<uint64_t> bytes_read_ = 0;

    /**
     * Bytes write
     */
    std::atomic<uint64_t> bytes_write_ = 0;

    // LCOV_EXCL_START

    /**
     * Constructor
     */
    subscription_metrics() = default;

    /**
     * Move
     *
     * @param other
     */
    subscription_metrics(subscription_metrics &&other) noexcept :
        bytes_read_{other.bytes_read_.load()}, bytes_write_{other.bytes_write_.load()}
    {
    }

    /**
     * Assignment
     * @param other
     * @return
     */
    subscription_metrics &operator=(subscription_metrics &&other) noexcept
    {
      bytes_read_.store(other.bytes_read_.load());
      bytes_write_.store(other.bytes_write_.load());
      return *this;
    }

    /**
     * Constructor
     */
    subscription_metrics(const subscription_metrics &) = delete;

    /**
     * Assignment
     *
     * @return
     */
    subscription_metrics &operator=(const subscription_metrics &) = delete;

    /**
     * Destructor
     */
    ~subscription_metrics() noexcept = default;
    // LCOV_EXCL_STOP
#endif
  };
} // namespace throttr

#endif // THROTTR_SUBSCRIPTION_METRICS_HPP
