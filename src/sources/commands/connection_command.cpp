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
#include <boost/uuid/uuid_io.hpp>
#include <throttr/connection.hpp>
#include <throttr/services/response_builder_service.hpp>
#include <throttr/state.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  void connection_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer,
    const boost::uuids::uuid id)
  {
    boost::ignore_unused(type, id);

    const auto _request = request_connection::from_buffer(view);
    const auto &_uuid = _request.id_;

    boost::uuids::uuid _id{};
    std::memcpy(_id.data, _uuid.data(), _uuid.size());

    bool _found_in_tcp = false;
    bool _found_in_unix = false;

    {
      std::lock_guard _lock(state->tcp_connections_mutex_);
      const auto &_tcp_map = state->tcp_connections_;
      _found_in_tcp = _tcp_map.contains(_id);
    }

    if (!_found_in_tcp)
    {
      std::lock_guard _lock(state->unix_connections_mutex_);
      const auto &_unix_map = state->unix_connections_;
      _found_in_unix = _unix_map.contains(_id);
    }

    if (!_found_in_tcp && !_found_in_unix)
    {
      batch.emplace_back(boost::asio::buffer(&state::failed_response_, 1));
      // LCOV_EXCL_START
#ifndef NDEBUG
      fmt::println(
        "{:%Y-%m-%d %H:%M:%S} REQUEST CONNECTION id={} from={} "
        "RESPONSE ok=false",
        std::chrono::system_clock::now(),
        span_to_hex(_request.id_),
        to_string(id));
#endif
      // LCOV_EXCL_STOP
      return;
    }

    if (_found_in_tcp)
    {
      std::lock_guard _lock(state->tcp_connections_mutex_);
      const auto &_tcp_map = state->tcp_connections_;
      const auto *_conn = _tcp_map.find(_id)->second;
      batch.emplace_back(boost::asio::buffer(&state::success_response_, 1));
      response_builder_service::write_connections_entry_to_buffer<tcp_socket>(state, &batch, _conn, write_buffer, false);
    }
    else
    {
      std::lock_guard _lock(state->unix_connections_mutex_);
      const auto &_unix_map = state->unix_connections_;
      const auto *_conn = _unix_map.find(_id)->second;
      batch.emplace_back(boost::asio::buffer(&state::success_response_, 1));
      response_builder_service::write_connections_entry_to_buffer<unix_socket>(state, &batch, _conn, write_buffer, false);
    }

    // LCOV_EXCL_START
#ifndef NDEBUG
    fmt::println(
      "{:%Y-%m-%d %H:%M:%S} REQUEST CONNECTION id={}from={} "
      "RESPONSE ok=true",
      std::chrono::system_clock::now(),
      span_to_hex(_request.id_),
      to_string(id));
#endif
    // LCOV_EXCL_STOP
  }
} // namespace throttr
