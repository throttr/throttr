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

#ifndef THROTTR_SESSION_HPP
#define THROTTR_SESSION_HPP

#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <span>
#include <throttr/connection_allocator.hpp>

namespace throttr
{
  /**
   * Forward state
   */
  class state;

  /**
   * Session
   */
  class connection : public std::enable_shared_from_this<connection>
  {
  public:
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

#ifndef NDEBUG
    /**
     * IP
     */
    std::string ip_ = "127.0.0.1";

    /**
     * Port
     */
    std::uint16_t port_ = 13579;
#endif
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
     * Get message sized
     *
     * @param buffer
     * @param header_size
     * @param extra
     * @return
     */
    static std::size_t get_message_sized(std::span<const std::byte> buffer, std::size_t header_size, std::size_t extra = 0);

    /**
     * Get message size
     *
     * @param buffer
     * @return
     */
    static std::size_t get_message_size(std::span<const std::byte> buffer);

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

#endif // THROTTR_SESSION_HPP
