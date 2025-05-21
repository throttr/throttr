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
#include <throttr/session.hpp>
#include <throttr/state.hpp>

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
    server(boost::asio::io_context &io_context, const short port, const std::shared_ptr<state> &state) :
        acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
        socket_(io_context),
        state_(state)
    {
      state->acceptor_ready_ = true;
      do_accept();
    };

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
          if (!error)
          {
            const auto _ptr = boost::intrusive_ptr{new session(std::move(socket_), state_)};
            _ptr->start();
          }

          do_accept();
        });
    }

    /**
     * Acceptor
     */
    boost::asio::ip::tcp::acceptor acceptor_;

    /**
     * Socket
     */
    boost::asio::ip::tcp::socket socket_;

    /**
     * State
     */
    std::shared_ptr<state> state_;
  };
} // namespace throttr

#endif // THROTTR_SERVER_HPP
