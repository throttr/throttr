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

#include <throttr/commands/unsubscribe_command.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <throttr/connection.hpp>
#include <throttr/services/subscriptions_service.hpp>
#include <throttr/state.hpp>
#include <throttr/time.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  void unsubscribe_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer,
    const boost::uuids::uuid id)
  {
    boost::ignore_unused(type, write_buffer);
    std::scoped_lock _lock(state->subscriptions_->mutex_);

    const auto _request = request_unsubscribe::from_buffer(view);

    std::string _channel(_request.channel_.size(), '\0');
    std::memcpy(_channel.data(), _request.channel_.data(), _request.channel_.size());

    batch.reserve(batch.size() + 1);

    if (!state->subscriptions_->is_subscribed(id, _channel))
    {
      batch.emplace_back(&state::failed_response_, 1);

#ifndef NDEBUG
      const std::vector _channel_bytes(_request.channel_.data(), _request.channel_.data() + _request.channel_.size());
      fmt::println(
        "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST UNSUBSCRIBE channel={} from={} "
        "RESPONSE ok=false",
        to_string(state->id_),
        std::chrono::system_clock::now(),
        span_to_hex(_channel_bytes),
        to_string(id));
#endif
      return;
    }

    auto &index = state->subscriptions_->subscriptions_.get<by_connection_id>();
    auto [begin, end] = index.equal_range(id);

    std::vector<decltype(index.begin())> _to_erase;

    for (auto it = begin; it != end; ++it)
    {
      if (it->channel_ == _channel)
        _to_erase.push_back(it);
    }

    for (const auto it : _to_erase)
    {
      index.erase(it);
    }

    batch.emplace_back(&state::success_response_, 1);

#ifndef NDEBUG
    const std::vector _channel_bytes(_request.channel_.begin(), _request.channel_.end());
    fmt::println(
      "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST UNSUBSCRIBE session_id={} META channel={} RESPONSE ok=true",
      to_string(state->id_),
      std::chrono::system_clock::now(),
      to_string(id),
      span_to_hex(_channel_bytes));
#endif
  }
} // namespace throttr
