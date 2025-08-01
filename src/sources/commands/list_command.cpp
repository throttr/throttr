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

#include <throttr/commands/list_command.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <throttr/connection.hpp>
#include <throttr/services/response_builder_service.hpp>
#include <throttr/state.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  void list_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer,
    const boost::uuids::uuid id)
  {

    boost::ignore_unused(type, view, id);

    response_builder_service::handle_fragmented_entries_response(
      state,
      batch,
      write_buffer,
      2048,
      [_state = state->shared_from_this(), _write_buffer_ref = std::ref(write_buffer)](
        std::vector<boost::asio::const_buffer> *b, const entry_wrapper *e, std::size_t &offset, const bool measure)
      { return response_builder_service::write_list_entry_to_buffer(_state, b, e, _write_buffer_ref, offset, measure); });

#ifndef NDEBUG
    fmt::println(
      "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST LIST session_id={} RESPONSE ok=true",
      to_string(state->id_),
      std::chrono::system_clock::now(),
      to_string(id));
#endif
  }
} // namespace throttr
