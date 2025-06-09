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

#ifndef THROTTR_AGENT_HPP
#define THROTTR_AGENT_HPP

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/intrusive_ptr.hpp>
#include <iostream>
#include <thread>
#include <throttr/connection.hpp>
#include <throttr/program_parameters.hpp>
#include <throttr/state.hpp>
#include <throttr/transport.hpp>

namespace throttr
{
  /**
   * Agent
   */
  class agent : public std::enable_shared_from_this<agent>
  {
  public:
    /**
     * Constructor
     *
     * @param io_context
     * @param program_options
     * @param state
     */
    agent(boost::asio::io_context &io_context, const program_parameters &program_options, const std::shared_ptr<state> &state) :
        tcp_socket_(io_context),
        tcp_resolver_(io_context),
        unix_socket_(io_context),
        state_(state),
        program_options_(program_options),
        strand_(io_context.get_executor())
    {
    }

    // LCOV_EXCL_START
    /**
     * Start
     */
    void start()
    {
      if (program_options_.has_master_)
        boost::asio::post(strand_, [_self = shared_from_this()]() mutable { _self->try_to_connect(); });
    }

    /**
     * Try to connect
     */
    void try_to_connect()
    {
      const bool _is_unix = program_options_.master_socket_ != "disabled";
      while (true)
      {
        if (const auto _success = _is_unix ? for_unix() : for_tcp(); _success)
          break;
        std::this_thread::sleep_for(std::chrono::seconds(5));
      }
    }

    /**
     * For UNIX
     *
     * @return
     */
    bool for_unix()
    {
      boost::system::error_code ec;
      const unix_endpoint _endpoint(program_options_.master_socket_);
      unix_socket_.connect(_endpoint, ec);
      if (ec)
      {
#ifndef NDEBUG
        fmt::println(
          "[{}] [{:%Y-%m-%d %H:%M:%S}] CONNECTION WITH MASTER UNIX SOCKET HAS BEEN FAILED ... RETRYING ...",
          to_string(state_->id_),
          std::chrono::system_clock::now());
#endif
        return false;
      }
      std::make_shared<connection<unix_socket>>(std::move(unix_socket_), state_, connection_type::agent)->start();
      return true;
    }

    /**
     * For TCP
     *
     * @return
     */
    bool for_tcp()
    {
      boost::system::error_code ec;
      const auto _endpoints = tcp_resolver_.resolve(program_options_.master_host_, std::to_string(program_options_.master_port_));
      boost::asio::connect(tcp_socket_, _endpoints, ec);
      if (ec)
      {
#ifndef NDEBUG
        fmt::println(
          "[{}] [{:%Y-%m-%d %H:%M:%S}] CONNECTION WITH MASTER TCP SOCKET HAS BEEN FAILED ... RETRYING ...",
          to_string(state_->id_),
          std::chrono::system_clock::now());
#endif
        return false;
      }

      std::make_shared<connection<tcp_socket>>(std::move(tcp_socket_), state_, connection_type::agent)->start();
      return true;
    }
    // LCOV_EXCL_STOP

  private:
    /**
     * TCP Socket
     */
    tcp_socket tcp_socket_;

    /**
     * TCP Resolver
     *
     * @return
     */
    boost::asio::ip::tcp::resolver tcp_resolver_;

    /**
     * UNIX Socket
     */
    unix_socket unix_socket_;

    /**
     * State
     */
    std::shared_ptr<state> state_;

    /**
     * Program options
     */
    program_parameters program_options_;

    /**
     * Strand
     */
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
  };
} // namespace throttr

#endif // THROTTR_AGENT_HPP
