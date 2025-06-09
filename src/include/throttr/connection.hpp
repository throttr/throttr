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

#ifndef THROTTR_CONNECTION_HPP
#define THROTTR_CONNECTION_HPP

#include <boost/uuid/uuid.hpp>
#include <deque>
#include <fmt/chrono.h>
#include <memory>
#include <span>
#include <throttr/connection_allocator.hpp>
#include <throttr/connection_metrics.hpp>
#include <throttr/message.hpp>
#include <throttr/state.hpp>

namespace throttr
{
  /**
   * Forward state
   */
  class state;

  /**
   * Connection kind
   */
  enum class connection_kind : uint8_t
  {
    tcp_socket,
    unix_socket,
  };

  /**
   * Connection type
   */
  enum class connection_type : uint8_t
  {
    client,
    agent,
  };

  /**
   * Connection
   */
  template<typename Transport> class connection : public std::enable_shared_from_this<connection<Transport>>
  {
  public:
    /**
     * ID
     */
    boost::uuids::uuid id_;

    /**
     * Connected at
     */
    std::uint64_t connected_at_ = 0;

    /**
     * Kind
     */
    connection_kind kind_;

    /**
     * Type
     */
    connection_type type_;

#ifdef ENABLED_FEATURE_METRICS
    /**
     * Metrics
     */
    std::shared_ptr<connection_metrics> metrics_ = std::make_shared<connection_metrics>();
#endif

    /**
     * Constructor
     *
     * @param socket
     * @param state
     * @param type
     */
    explicit connection(Transport socket, const std::shared_ptr<state> &state, connection_type type);

    /**
     * Destructor
     */
    ~connection();

    /**
     * Start
     */
    void start();

    /**
     * Compact buffer if needed
     */
    void compact_buffer_if_needed();

    /**
     * Compact buffer
     */
    void compact_buffer();

    /**
     * Max length
     */
    static constexpr std::size_t max_length_ = 8096;

    /**
     * Buffer
     */
    std::array<std::byte, max_length_> buffer_;

    /**
     * Buffer start
     */
    std::size_t buffer_start_ = 0;

    /**
     * Buffer end
     */
    std::size_t buffer_end_ = 0;

    /**
     * IP
     */
    std::string ip_ = "127.0.0.1";

    /**
     * Port
     */
    std::uint16_t port_ = 13579;

    /**
     * Send
     *
     * @param batch
     */
    void send(std::shared_ptr<message> batch);

  private:
    /**
     * Handler memory
     */
    connection_handler_memory handler_memory_;

    /**
     * On read
     *
     * @param error
     * @param length
     */
    void on_read(const boost::system::error_code &error, std::size_t length);

    /**
     * Try process next
     */
    void try_process_next();

    /**
     * Get message size
     *
     * @param buffer
     * @return
     */
    std::size_t get_message_size(std::span<const std::byte> buffer) const;

    /**
     * Do read
     */
    void do_read();

    /**
     * On write
     *
     * @param error
     * @param length
     */
    void on_write(const boost::system::error_code &error, std::size_t length);

    /**
     * Close
     */
    void close();

    /**
     * Socket
     */
    Transport socket_;

    /**
     * State
     */
    std::shared_ptr<state> state_;

    /**
     * Pending writes
     */
    std::deque<std::shared_ptr<message>> pending_writes_;

    /**
     * Messages pool
     */
    std::shared_ptr<message> message_ = std::make_shared<message>();

    /**
     * On send
     *
     * @param batch
     */
    void on_send(const std::shared_ptr<message> &batch);

    /**
     * Write next
     */
    void write_next();
  };
} // namespace throttr

#include <throttr/connection.ipp> // NOSONAR

#endif // THROTTR_CONNECTION_HPP
