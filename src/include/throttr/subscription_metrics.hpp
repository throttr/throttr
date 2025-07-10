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

#include <boost/core/ignore_unused.hpp>
#include <throttr/metric.hpp>

namespace throttr
{
  struct subscription_metrics // NOSONAR
  {
#ifdef ENABLED_FEATURE_METRICS
    /**
     * Read bytes
     */
    metric read_bytes_;

    /**
     * Write bytes
     */
    metric write_bytes_;

    /**
     * Constructor
     */
    subscription_metrics() = default;

    // Non copiable and movable
    subscription_metrics(const subscription_metrics &) = delete;
    subscription_metrics &operator=(const subscription_metrics &) = delete;
    subscription_metrics(subscription_metrics &&) = delete;
    subscription_metrics &operator=(subscription_metrics &&) = delete;

    /**
     * Destructor
     */
    ~subscription_metrics() noexcept = default;
#endif
  };
} // namespace throttr

#endif // THROTTR_SUBSCRIPTION_METRICS_HPP
