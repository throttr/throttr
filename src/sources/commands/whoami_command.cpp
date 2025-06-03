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

#include <throttr/commands/whoami_command.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <throttr/connection.hpp>
#include <throttr/services/response_builder_service.hpp>
#include <throttr/state.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  void whoami_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer,
    const std::shared_ptr<connection> &conn)
  {
    boost::ignore_unused(state, type, view);

    batch.emplace_back(boost::asio::buffer(&state::success_response_, 1));

    {
      const auto _offset = write_buffer.size();
      const auto *id_data = reinterpret_cast<const std::byte *>(conn->id_.data()); // NOSONAR
      write_buffer.insert(write_buffer.end(), id_data, id_data + 16);
      batch.emplace_back(boost::asio::buffer(reinterpret_cast<const void *>(&write_buffer[_offset]), 16)); // NOSONAR
    }

    // LCOV_EXCL_START
#ifndef NDEBUG
    fmt::println(
      "{:%Y-%m-%d %H:%M:%S} REQUEST WHOAMI from={} "
      "RESPONSE ok=true",
      std::chrono::system_clock::now(),
      to_string(conn->id_));
#endif
    // LCOV_EXCL_STOP
  }
} // namespace throttr
