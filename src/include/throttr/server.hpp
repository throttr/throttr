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
#include <throttr/program_options.hpp>
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
    server(boost::asio::io_context &io_context, const program_options &program_options, const std::shared_ptr<state> &state) :
        acceptor_(io_context, make_endpoint(program_options)), socket_(io_context), state_(state)
    {
#ifndef ENABLED_FEATURE_UNIX_SOCKETS
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
     * @param program_options
     * @return transport_endpoint
     */
    static local_transport_endpoint make_endpoint(const program_options &program_options)
    {
#ifdef ENABLED_FEATURE_UNIX_SOCKETS
      return {program_options.socket_};
#else
      return {boost::asio::ip::tcp::v4(), static_cast<boost::asio::ip::port_type>(program_options.port_)};
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
            std::make_shared<connection<local_transport_socket>>(std::move(socket_), state_)->start();
          }

          do_accept();
        });
    }

    /**
     * Acceptor
     */
    local_transport_acceptor acceptor_;

    /**
     * Socket
     */
    local_transport_socket socket_;

    /**
     * State
     */
    std::shared_ptr<state> state_;
  };
} // namespace throttr

#endif // THROTTR_SERVER_HPP
