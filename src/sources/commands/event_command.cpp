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

#include <throttr/commands/event_command.hpp>

#include <boost/uuid/uuid_io.hpp>

#include <throttr/connection.hpp>
#include <throttr/services/create_service.hpp>
#include <throttr/state.hpp>

namespace throttr
{
  void event_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer,
    const boost::uuids::uuid id)
  {
    boost::ignore_unused(type);

    const auto _request = request_event::from_buffer(view);
    const auto _type = static_cast<request_types>(std::to_integer<uint8_t>(_request.buffer_[0]));

    batch.reserve(batch.size() + 1);

#ifdef ENABLED_FEATURE_METRICS
    state->metrics_collector_->commands_[static_cast<std::size_t>(_type)].mark();
#endif

#ifndef NDEBUG
    fmt::println(
      "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST EVENT session_id={} META channel={} payload={} RESPONSE ok=true",
      to_string(state->id_),
      std::chrono::system_clock::now(),
      to_string(id),
      span_to_hex(_request.channel_),
      span_to_hex(_request.buffer_));
#endif

    batch.emplace_back(&state::success_response_, 1);

    state->commands_->commands_[static_cast<std::size_t>(_type)](state, _type, _request.buffer_, batch, write_buffer, id);
  }
} // namespace throttr
