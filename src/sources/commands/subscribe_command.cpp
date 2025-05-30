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
    std::vector<std::uint8_t> &write_buffer,
    const std::shared_ptr<connection> &conn)
  {
    boost::ignore_unused(type, write_buffer);

    const auto _request = request_subscribe::from_buffer(view);
    if (state->subscriptions_->is_subscribed(conn->id_, _request.channel_)) // LCOV_EXCL_LINE Note: Partially tested.
    {
      batch.emplace_back(boost::asio::buffer(&state::failed_response_, 1));
      return;
    }

    const std::vector<std::byte> channel_bytes{
      reinterpret_cast<const std::byte *>(_request.channel_.data()),                             // NOSONAR
      reinterpret_cast<const std::byte *>(_request.channel_.data() + _request.channel_.size())}; // NOSONAR

    state->subscriptions_->subscriptions_.insert(subscription{conn->id_, channel_bytes});

    batch.emplace_back(boost::asio::buffer(&state::success_response_, 1));
  }
} // namespace throttr
