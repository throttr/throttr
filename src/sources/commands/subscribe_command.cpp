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

#include <throttr/commands/subscribe_command.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <throttr/connection.hpp>
#include <throttr/services/subscriptions_service.hpp>
#include <throttr/state.hpp>
#include <throttr/time.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  void subscribe_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer,
    const boost::uuids::uuid id)
  {
    boost::ignore_unused(type, write_buffer);
    std::scoped_lock _lock(state->subscriptions_->mutex_);

    const auto _request = request_subscribe::from_buffer(view);
    const std::string _channel{
      std::string_view(reinterpret_cast<const char *>(_request.channel_.data()), _request.channel_.size())}; // NOSONAR

    auto [_it, _inserted] = state->subscriptions_->subscriptions_.insert(subscription{id, _channel});

    batch.reserve(batch.size() + 1);

    batch.emplace_back(_inserted ? &state::success_response_ : &state::failed_response_, 1);

#ifndef NDEBUG
    const auto _channel_view =
      std::string_view(reinterpret_cast<const char *>(_request.channel_.data()), _request.channel_.size()); // NOSONAR
    fmt::println(
      "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST SUBSCRIBE session_id={} META channel={} RESPONSE ok={}",
      to_string(state->id_),
      std::chrono::system_clock::now(),
      to_string(id),
      _channel_view,
      _inserted);
#endif
  }
} // namespace throttr
