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

namespace throttr
{
#ifdef ENABLED_FEATURE_METRICS
  struct connection_metrics
  {
    /**
     * Read bytes
     */
    std::atomic<uint64_t> read_bytes_ = 0;

    /**
     * Write bytes
     */
    std::atomic<uint64_t> write_bytes_ = 0;

    /**
     * Published bytes
     */
    std::atomic<uint64_t> published_bytes_ = 0;

    /**
     * Received bytes
     */
    std::atomic<uint64_t> received_bytes_ = 0;

    /**
     * Allocated bytes
     */
    std::atomic<uint64_t> allocated_bytes_ = 0;

    /**
     * Consumed bytes
     */
    std::atomic<uint64_t> consumed_bytes_ = 0;

    /**
     * INSERT requests
     */
    std::atomic<uint64_t> insert_request_ = 0;

    /**
     * SET requests
     */
    std::atomic<uint64_t> set_request_ = 0;

    /**
     * QUERY requests
     */
    std::atomic<uint64_t> query_request_ = 0;

    /**
     * GET requests
     */
    std::atomic<uint64_t> get_request_ = 0;

    /**
     * UPDATE requests
     */
    std::atomic<uint64_t> update_request_ = 0;

    /**
     * PURGE requests
     */
    std::atomic<uint64_t> purge_request_ = 0;

    /**
     * LIST requests
     */
    std::atomic<uint64_t> list_request_ = 0;

    /**
     * INFO requests
     */
    std::atomic<uint64_t> info_request_ = 0;

    /**
     * STAT requests
     */
    std::atomic<uint64_t> stat_request_ = 0;

    /**
     * STATS requests
     */
    std::atomic<uint64_t> stats_request_ = 0;

    /**
     * SUBSCRIBE requests
     */
    std::atomic<uint64_t> subscribe_request_ = 0;

    /**
     * UNSUBSCRIBE requests
     */
    std::atomic<uint64_t> unsubscribe_request_ = 0;

    /**
     * CONNECTIONS requests
     */
    std::atomic<uint64_t> connections_request_ = 0;

    /**
     * CONNECTION requests
     */
    std::atomic<uint64_t> connection_request_ = 0;

    /**
     * CHANNELS requests
     */
    std::atomic<uint64_t> channels_request_ = 0;

    /**
     * CHANNEL requests
     */
    std::atomic<uint64_t> channel_request_ = 0;

    /**
     * WHOAMI requests
     */
    std::atomic<uint64_t> whoami_request_ = 0;
  };
#endif
} // namespace throttr

#endif // THROTTR_CONNECTION_METRICS_HPP
