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

#include <throttr/connection.hpp>

#include <throttr/state.hpp>

namespace throttr
{
  connection::connection(transport_socket socket, const std::shared_ptr<state> &state) :
      id_(state->id_generator_()), socket_(std::move(socket)), state_(state)
  {
    // LCOV_EXCL_START
    if (socket_.is_open())
    {
#ifndef ENABLED_FEATURE_UNIX_SOCKETS
      const boost::asio::ip::tcp::no_delay no_delay_option(true);
      socket_.set_option(no_delay_option);
      ip_ = socket_.remote_endpoint().address().to_string();
      port_ = socket_.remote_endpoint().port();
#endif
      connected_at_ =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
    // LCOV_EXCL_STOP
  }

} // namespace throttr