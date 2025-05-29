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

#include <boost/asio/ip/tcp.hpp>
#include <boost/uuid/uuid.hpp>
#include <memory>
#include <span>
#include <throttr/connection_allocator.hpp>
#include <throttr/connection_metrics.hpp>

namespace throttr
{
  /**
   * Forward state
   */
  class state;

  /**
   * Connection
   */
  class connection : public std::enable_shared_from_this<connection>
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
     */
    explicit connection(boost::asio::ip::tcp::socket socket, const std::shared_ptr<state> &state);

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
     * Do write
     */
    void do_write(const std::vector<boost::asio::const_buffer> &batch);

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
    boost::asio::ip::tcp::socket socket_;

    /**
     * State
     */
    std::shared_ptr<state> state_;

    /**
     * Write buffer
     */
    std::vector<uint8_t> write_buffer_;
  };
} // namespace throttr

#endif // THROTTR_CONNECTION_HPP
