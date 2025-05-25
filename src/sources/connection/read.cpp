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

#include <boost/asio/bind_allocator.hpp>

namespace throttr
{
  void connection::do_read()
  {
    // LCOV_EXCL_START
    if (buffer_end_ == max_length_)
    {
      compact_buffer();
    }
    // LCOV_EXCL_STOP

    auto _self = shared_from_this();
    socket_.async_read_some(
      boost::asio::buffer(buffer_.data() + buffer_end_, max_length_ - buffer_end_),
      boost::asio::bind_allocator(
        connection_handler_allocator<int>(handler_memory_),
        [_self](const boost::system::error_code &ec, const std::size_t length) { _self->on_read(ec, length); }));
  }

  void connection::on_read(const boost::system::error_code &error, std::size_t length)
  {
    if (!error) // LCOV_EXCL_LINE note: Partially tested.
    {
      buffer_end_ += length;
      try_process_next();
      // LCOV_EXCL_START
    }
    else
    {
      close();
    }
    // LCOV_EXCL_STOP
  }
} // namespace throttr