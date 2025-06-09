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

#ifndef THROTTR_SERVER_HPP
#define THROTTR_SERVER_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/intrusive_ptr.hpp>
#include <throttr/connection.hpp>
#include <throttr/program_parameters.hpp>
#include <throttr/state.hpp>
#include <throttr/transport.hpp>

namespace throttr
{
  /**
   * Server
   */
  class server
  {
  public:
    /**
     * Constructor
     *
     * @param io_context
     * @param program_options
     * @param state
     */
    server(boost::asio::io_context &io_context, const program_parameters &program_options, const std::shared_ptr<state> &state) :
        tcp_acceptor_(
          io_context,
          tcp_endpoint{boost::asio::ip::tcp::v4(), static_cast<boost::asio::ip::port_type>(program_options.port_)}),
        tcp_socket_(io_context),
        unix_acceptor_(io_context, unix_endpoint{program_options.socket_.c_str()}),
        unix_socket_(io_context),
        state_(state)
    {
      state->exposed_port_ = tcp_acceptor_.local_endpoint().port();
      state->exposed_socket_ = program_options.socket_;
      state->acceptor_ready_ = true;

#ifdef ENABLED_FEATURE_METRICS
      state->metrics_collector_->schedule_timer(state);
#endif

      do_tcp_accept();
      do_unix_accept();
    }

  private:
    /**
     * Do TCP accept
     */
    void do_tcp_accept()
    {
      tcp_acceptor_.async_accept(
        tcp_socket_,
        [this](const boost::system::error_code &error)
        {
          if (!error) // LCOV_EXCL_LINE Note: Partially tested.
          {
            std::make_shared<connection<tcp_socket>>(std::move(tcp_socket_), state_, connection_type::client)->start();
          }

          do_tcp_accept();
        });
    }

    /**
     * Do UNIX accept
     */
    void do_unix_accept()
    {
      unix_acceptor_.async_accept(
        unix_socket_,
        [this](const boost::system::error_code &error)
        {
          if (!error) // LCOV_EXCL_LINE Note: Partially tested.
          {
            std::make_shared<connection<unix_socket>>(std::move(unix_socket_), state_, connection_type::client)->start();
          }

          do_unix_accept();
        });
    }

    /**
     * TCP Acceptor
     */
    tcp_acceptor tcp_acceptor_;

    /**
     * TCP Socket
     */
    tcp_socket tcp_socket_;

    /**
     * UNIX Acceptor
     */
    unix_acceptor unix_acceptor_;

    /**
     * UNIX Socket
     */
    unix_socket unix_socket_;

    /**
     * State
     */
    std::shared_ptr<state> state_;
  };
} // namespace throttr

#endif // THROTTR_SERVER_HPP
