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

namespace throttr
{
  connection::connection(boost::asio::ip::tcp::socket socket, const std::shared_ptr<state> &state) :
      socket_(std::move(socket)), state_(state)
  {
    // LCOV_EXCL_START
    if (socket_.is_open())
    {
      const boost::asio::ip::tcp::no_delay no_delay_option(true);
      socket_.set_option(no_delay_option);
#ifndef NDEBUG
      ip_ = socket_.remote_endpoint().address().to_string();
      port_ = socket_.remote_endpoint().port();
#endif
      write_buffer_.reserve(max_length_);
    }
    // LCOV_EXCL_STOP
  }

} // namespace throttr