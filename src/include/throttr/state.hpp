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

#ifndef THROTTR_STATE_HPP
#define THROTTR_STATE_HPP

#include <atomic>
#include <boost/asio/buffer.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/uuid/random_generator.hpp>
#include <cstring>
#include <deque>
#include <memory>
#include <throttr/protocol_wrapper.hpp>
#include <throttr/services/commands_service.hpp>
#include <throttr/services/find_service.hpp>
#include <throttr/services/garbage_collector_service.hpp>
#include <throttr/services/messages_service.hpp>
#include <throttr/services/metrics_collector_service.hpp>
#include <throttr/services/response_builder_service.hpp>
#include <throttr/storage.hpp>
#include <vector>

namespace throttr
{
  /**
   * Forward connection
   */
  class connection;

  /**
   * State
   */
  class state : public std::enable_shared_from_this<state>
  {
  public:
    /**
     * Acceptor ready
     */
    std::atomic_bool acceptor_ready_;

    /**
     * Port exposed
     */
    std::uint16_t exposed_port_;

    /**
     * Storage
     */
    storage_type storage_;

    /**
     * Expiration timer
     */
    boost::asio::steady_timer expiration_timer_;

    /**
     * UUID generator
     */
    boost::uuids::random_generator id_generator_ = boost::uuids::random_generator();

    /**
     * Connections container
     */
    std::unordered_map<boost::uuids::uuid, connection *, boost::hash<boost::uuids::uuid>> connections_;

    /**
     * Connections mutex
     */
    std::mutex connections_mutex_;

#ifdef ENABLED_FEATURE_METRICS
    /**
     * Metrics timer
     */
    boost::asio::steady_timer metrics_timer_;
#endif

    /**
     * Strand
     */
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

    /**
     * Scheduled key
     */
    std::span<const std::byte> scheduled_key_;

    /**
     * Remove lock
     */
    std::mutex mutex_;

    /**
     * Success response
     */
    constexpr static uint8_t success_response_ = 0x01;

    /**
     * Failed response
     */
    constexpr static uint8_t failed_response_ = 0x00;

    /**
     * Expired entries
     */
    std::deque<std::pair<entry_wrapper, std::chrono::steady_clock::time_point>> expired_entries_;

    /**
     * Commands service
     */
    std::shared_ptr<commands_service> commands_ = std::make_shared<commands_service>();

    /**
     * Messages service
     */
    std::shared_ptr<messages_service> messages_ = std::make_shared<messages_service>();

    /**
     * Find service
     */
    std::shared_ptr<find_service> finder_ = std::make_shared<find_service>();

    /**
     * Response builder service
     */
    std::shared_ptr<response_builder_service> response_builder_ = std::make_shared<response_builder_service>();

    /**
     * Garbage collector service
     */
    std::shared_ptr<garbage_collector_service> garbage_collector_ = std::make_shared<garbage_collector_service>();

#ifdef ENABLED_FEATURE_METRICS
    /**
     * Metrics collector service
     */
    std::shared_ptr<metrics_collector_service> metrics_collector_ = std::make_shared<metrics_collector_service>();
#endif

    /**
     * Constructor
     *
     * @param ioc
     */
    explicit state(boost::asio::io_context &ioc);

    /**
     * Join
     *
     * @param connection
     */
    void join(connection *connection);

    /**
     * Leave
     *
     * @param connection
     */
    void leave(const connection *connection);
  };
} // namespace throttr

#endif // THROTTR_STATE_HPP
