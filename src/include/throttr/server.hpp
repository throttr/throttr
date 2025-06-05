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
     * @param port
     * @param state
     */
    server(
      boost::asio::io_context &io_context,
#ifdef ENABLED_FEATURE_UNIX_SOCKETS
      const std::string &port,
#else
      const short port,
#endif
      const std::shared_ptr<state> &state) :
        acceptor_(io_context, make_endpoint(port)), socket_(io_context), state_(state)
    {
#ifdef ENABLED_FEATURE_UNIX_SOCKETS
      state->exposed_port_ = port;
#else
      state->exposed_port_ = acceptor_.local_endpoint().port();
#endif
      state->acceptor_ready_ = true;

#ifdef ENABLED_FEATURE_METRICS
      state->metrics_collector_->schedule_timer(state);
#endif

      do_accept();
    }

    /**
     * Make endpoint
     *
     * @param port
     * @return transport_endpoint
     */
    static transport_endpoint make_endpoint(
#ifdef ENABLED_FEATURE_UNIX_SOCKETS
      const std::string &port
#else
      const short port
#endif
    )
    {
#ifdef ENABLED_FEATURE_UNIX_SOCKETS
      return {port};
#else
      return {boost::asio::ip::tcp::v4(), static_cast<boost::asio::ip::port_type>(port)};
#endif
    }

  private:
    /**
     * Do accept
     */
    void do_accept()
    {
      acceptor_.async_accept(
        socket_,
        [this](const boost::system::error_code &error)
        {
          if (!error) // LCOV_EXCL_LINE Note: Partially tested.
          {
            std::make_shared<connection>(std::move(socket_), state_)->start();
          }

          do_accept();
        });
    }

    /**
     * Acceptor
     */
    transport_acceptor acceptor_;

    /**
     * Socket
     */
    transport_socket socket_;

    /**
     * State
     */
    std::shared_ptr<state> state_;
  };
} // namespace throttr

#endif // THROTTR_SERVER_HPP
