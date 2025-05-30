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

#ifndef THROTTR_CONNECTION_METRICS_HPP
#define THROTTR_CONNECTION_METRICS_HPP

#include <atomic>
#include <throttr/metric.hpp>

namespace throttr
{
#ifdef ENABLED_FEATURE_METRICS
  /**
   * Connection network metrics
   */
  struct connection_network_metrics
  {
    /**
     * Read bytes
     */
    metric read_bytes_;

    /**
     * Write bytes
     */
    metric write_bytes_;

    /**
     * Published bytes
     */
    metric published_bytes_;

    /**
     * Received bytes
     */
    metric received_bytes_;
  };

  /**
   * Connection memory metrics
   */
  struct connection_memory_metrics
  {
    /**
     * Allocated bytes
     */
    metric allocated_bytes_;

    /**
     * Consumed bytes
     */
    metric consumed_bytes_;
  };

  /**
   * Connection metrics
   */
  struct connection_metrics
  {
    /**
     * Network metrics
     */
    connection_network_metrics network_;

    /**
     * Memory metrics
     */
    connection_memory_metrics memory_;

    /**
     * Commands metrics
    */
    std::array<metric, 32> commands_{};
  };
#endif
} // namespace throttr

#endif // THROTTR_CONNECTION_METRICS_HPP
