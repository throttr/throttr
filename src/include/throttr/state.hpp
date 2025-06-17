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

#include "transport.hpp"
#include <throttr/services/subscriptions_service.hpp>

#include <atomic>
#include <boost/asio/buffer.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/uuid/random_generator.hpp>
#include <cstring>
#include <deque>
#include <memory>
#include <throttr/connection_type.hpp>
#include <throttr/program_parameters.hpp>
#include <throttr/protocol_wrapper.hpp>
#include <throttr/storage.hpp>

#include <vector>

namespace throttr
{
  template<typename Transport> class connection;

  /**
   * Forward response builder service
   */
  class response_builder_service;

  /**
   * Forward command service
   */
  class commands_service;

  /**
   * Forward subscriptions service
   */
  class subscriptions_service;

  /**
   * Forward messages service
   */
  class messages_service;

  /**
   * Forward find service
   */
  class find_service;

  /**
   * Forward garbage collector service
   */
  class garbage_collector_service;

#ifdef ENABLED_FEATURE_METRICS
  /**
   * Forward metrics collector service
   */
  class metrics_collector_service;
#endif

  /**
   * State
   */
  class state : public std::enable_shared_from_this<state> // NOSONAR Too many fields
  {
  public:
    /**
     * ID
     */
    boost::uuids::uuid id_ = boost::uuids::random_generator()();

    /**
     * Started at
     */
    std::uint64_t started_at_ = 0;

    /**
     * Acceptor ready
     */
    std::atomic_bool acceptor_ready_;

    /**
     * Port exposed
     */
    std::uint16_t exposed_port_;

    /**
     * Socket exposed
     */
    std::string exposed_socket_;

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
     * TCP Connections container
     */
    std::unordered_map<boost::uuids::uuid, connection<tcp_socket> *, std::hash<boost::uuids::uuid>> tcp_connections_;

    /**
     * UNIX Connections container
     */
    std::unordered_map<boost::uuids::uuid, connection<unix_socket> *, std::hash<boost::uuids::uuid>> unix_connections_;

    /**
     * Agent TCP Connections container
     */
    std::unordered_map<boost::uuids::uuid, connection<tcp_socket> *, std::hash<boost::uuids::uuid>> agent_tcp_connections_;

    /**
     * Agent UNIX Connections container
     */
    std::unordered_map<boost::uuids::uuid, connection<unix_socket> *, std::hash<boost::uuids::uuid>> agent_unix_connections_;

    /**
     * Connections mutex
     */
    std::mutex tcp_connections_mutex_;

    /**
     * UNIX Connections mutex
     */
    std::mutex unix_connections_mutex_;

    /**
     * Agent Connections mutex
     */
    std::mutex agent_tcp_connections_mutex_;

    /**
     * Agent UNIX Connections mutex
     */
    std::mutex agent_unix_connections_mutex_;

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
    std::deque<std::pair<entry_wrapper, std::chrono::system_clock::time_point>> expired_entries_;

    /**
     * Commands service
     */
    std::shared_ptr<commands_service> commands_;

    /**
     * Subscriptions service
     */
    std::shared_ptr<subscriptions_service> subscriptions_;

    /**
     * Messages service
     */
    std::shared_ptr<messages_service> messages_;

    /**
     * Find service
     */
    std::shared_ptr<find_service> finder_;

    /**
     * Response builder service
     */
    std::shared_ptr<response_builder_service> response_builder_;

    /**
     * Garbage collector service
     */
    std::shared_ptr<garbage_collector_service> garbage_collector_;

#ifdef ENABLED_FEATURE_METRICS
    /**
     * Metrics collector service
     */
    std::shared_ptr<metrics_collector_service> metrics_collector_;
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
    template<typename Transport> void join(connection<Transport> *connection)
    {
      if constexpr (std::is_same_v<Transport, tcp_socket>)
      {
        if (connection->type_ == connection_type::client) // LCOV_EXCL_LINE
        {
          std::scoped_lock lock(tcp_connections_mutex_);
          tcp_connections_.try_emplace(connection->id_, connection);
        }
        else
        {
          // LCOV_EXCL_START
          std::scoped_lock lock(agent_tcp_connections_mutex_);
          agent_tcp_connections_.try_emplace(connection->id_, connection);
          // LCOV_EXCL_STOP
        }
      }
      else
      {
        if (connection->type_ == connection_type::client)
        {
          std::scoped_lock lock(unix_connections_mutex_);
          unix_connections_.try_emplace(connection->id_, connection);
        }
        else
        {
          std::scoped_lock lock(agent_unix_connections_mutex_);
          agent_unix_connections_.try_emplace(connection->id_, connection);
        }
      }

      {
        std::scoped_lock lock(subscriptions_->mutex_);
        std::ostringstream oss;
        for (auto byte : connection->id_) {
          oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }
        const std::string _id(oss.str());
        subscriptions_->subscriptions_.insert(subscription{connection->id_, _id});
        subscriptions_->subscriptions_.insert(subscription{connection->id_, "*"});
      }
    }

    /**
     * Leave
     *
     * @param connection
     */
    template<typename Transport> void leave(const connection<Transport> *connection)
    {
      {
        std::scoped_lock _lock(subscriptions_->mutex_);
        auto &subs = subscriptions_->subscriptions_.get<by_connection_id>();
        auto [begin, end] = subs.equal_range(connection->id_);
        subs.erase(begin, end);
      }

      if constexpr (std::is_same_v<Transport, tcp_socket>)
      {
        if (connection->type_ == connection_type::client) // LCOV_EXCL_LINE
        {
          std::scoped_lock _lock(tcp_connections_mutex_);
          tcp_connections_.erase(connection->id_);
        }
        else
        {
          // LCOV_EXCL_START
          std::scoped_lock _lock(agent_tcp_connections_mutex_);
          agent_tcp_connections_.erase(connection->id_);
          // LCOV_EXCL_STOP
        }
      }
      else
      {
        if (connection->type_ == connection_type::client)
        {
          std::scoped_lock _lock(unix_connections_mutex_);
          unix_connections_.erase(connection->id_);
        }
        else
        {
          std::scoped_lock _lock(agent_unix_connections_mutex_);
          agent_unix_connections_.erase(connection->id_);
        }
      }
    }

    /**
     * Prepare for shutdown
     */
    void prepare_for_shutdown(const program_parameters &parameters);

    /**
     * Prepare for startup
     */
    void prepare_for_startup(const program_parameters &parameters);
  };
} // namespace throttr

#endif // THROTTR_STATE_HPP
