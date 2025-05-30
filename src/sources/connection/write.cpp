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
#include <boost/asio/write.hpp>
#include <boost/core/ignore_unused.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  void connection::on_write(const boost::system::error_code &error, const std::size_t length)
  {
    boost::ignore_unused(length);

    // LCOV_EXCL_START
    if (error)
    {
      close();
      return;
    }

    pending_writes_.erase(pending_writes_.begin());

    if (!pending_writes_.empty())
    {

      // LCOV_EXCL_START
#ifndef NDEBUG
      fmt::println(
        "{:%Y-%m-%d %H:%M:%S} SESSION WRITE ip={} port={} buffer={}",
        std::chrono::system_clock::now(),
        ip_,
        port_,
        buffers_to_hex(pending_writes_.front()->buffers_));
#endif
      // LCOV_EXCL_STOP

      boost::asio::async_write(
        socket_,
        pending_writes_.front()->buffers_,
        boost::asio::bind_allocator(
          connection_handler_allocator<int>(handler_memory_),
          [_self = shared_from_this()](const boost::system::error_code &ec, const std::size_t length)
          { _self->on_write(ec, length); }));
    }
    // LCOV_EXCL_STOP
  }
} // namespace throttr