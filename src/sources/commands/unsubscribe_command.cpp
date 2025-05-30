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
#include <throttr/services/subscriptions_service.hpp>
#include <throttr/state.hpp>
#include <throttr/time.hpp>

namespace throttr
{
  void unsubscribe_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::uint8_t> &write_buffer,
    const boost::uuids::uuid id)
  {
    boost::ignore_unused(type, write_buffer);

    const auto _request = request_unsubscribe::from_buffer(view);

    if (!state->subscriptions_->is_subscribed(id, _request.channel_))
    {
      batch.emplace_back(boost::asio::buffer(&state::failed_response_, 1));
      return;
    }

    auto &index = state->subscriptions_->subscriptions_.get<by_connection_id>();
    auto [begin, end] = index.equal_range(id);

    for (auto it = begin; it != end;)
    {
      if (it->channel() == _request.channel_)
        it = index.erase(it);
      else
        ++it;
    }

    batch.emplace_back(boost::asio::buffer(&state::success_response_, 1));
  }
} // namespace throttr
