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

#include <throttr/commands/stats_command.hpp>

#include <boost/core/ignore_unused.hpp>
#include <throttr/state.hpp>
#include <throttr/utils.hpp>

#include <throttr/connection.hpp>

namespace throttr
{
  void stats_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::uint8_t> &write_buffer,
    const std::shared_ptr<connection> &conn)
  {

    boost::ignore_unused(type, view, conn);

#ifndef ENABLED_FEATURE_METRICS
    batch.emplace_back(boost::asio::buffer(&failed_response_, 1));
    return;
#endif

    response_builder_service::handle_fragmented_entries_response(
      state,
      batch,
      write_buffer,
      2048,
      [_state = state->shared_from_this(),
       &write_buffer](std::vector<boost::asio::const_buffer> *b, const entry_wrapper *e, const bool measure)
      { return _state->response_builder_->write_stats_entry_to_buffer(_state, b, e, write_buffer, measure); });

    // LCOV_EXCL_START
#ifndef NDEBUG
    fmt::println(
      "{:%Y-%m-%d %H:%M:%S} REQUEST STATS id={} "
      "RESPONSE ok=true",
      std::chrono::system_clock::now(),
      id_to_hex(conn->id_));
#endif
    // LCOV_EXCL_STOP
  }
} // namespace throttr
