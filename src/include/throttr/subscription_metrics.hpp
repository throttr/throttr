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
  struct subscription_metrics // NOSONAR
  {
#ifdef ENABLED_FEATURE_METRICS
    /**
     * Read bytes
     */
    std::atomic<uint64_t> read_bytes_ = 0;

    /**
     * Write bytes
     */
    std::atomic<uint64_t> write_bytes_ = 0;

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
        // NOSONAR
        read_bytes_{other.read_bytes_.load()},
        write_bytes_{other.write_bytes_.load()}
    {
    }

    /**
     * Assignment
     * @param other
     * @return
     */
    subscription_metrics &operator=(subscription_metrics &&other) noexcept
    {
      read_bytes_.store(other.read_bytes_.load());
      write_bytes_.store(other.write_bytes_.load());
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
    // LCOV_EXCL_STOP
#endif
    /**
     * Destructor
     */
    ~subscription_metrics() noexcept = default;
  };
} // namespace throttr

#endif // THROTTR_SUBSCRIPTION_METRICS_HPP
