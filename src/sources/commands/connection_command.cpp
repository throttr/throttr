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

#include <throttr/commands/connection_command.hpp>

#include <boost/core/ignore_unused.hpp>
#include <throttr/services/response_builder_service.hpp>
#include <throttr/state.hpp>

namespace throttr
{
  void connection_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::uint8_t> &write_buffer,
    boost::uuids::uuid id)
  {
    boost::ignore_unused(type, id);

    const auto _request = request_connection::from_buffer(view);
    const auto &_uuid = _request.header_->id_;

    std::lock_guard _lock(state->connections_mutex_);
    const auto &_map = state->connections_;
    const auto _it = _map.find(_uuid);

    if (_it == _map.end())
    {
      batch.emplace_back(boost::asio::buffer(&state::failed_response_, 1));
      return;
    }

    const connection *_conn = _it->second;
    batch.emplace_back(boost::asio::buffer(&state::success_response_, 1));
    state->response_builder_->write_connections_entry_to_buffer(state, &batch, _conn, write_buffer, false);
  }
} // namespace throttr
