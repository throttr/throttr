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

#include <throttr/state.hpp>

#include <throttr/connection.hpp>

namespace throttr
{
  void state::join(connection *connection)
  {
    std::lock_guard lock(connections_mutex_);
    connections_.try_emplace(connection->id_, connection);
  }

  void state::leave(const connection *connection)
  {
    std::scoped_lock _lock(connections_mutex_, subscriptions_->mutex_);
    auto &subs = subscriptions_->subscriptions_.get<by_connection_id>();
    auto [begin, end] = subs.equal_range(connection->id_);
    subs.erase(begin, end);
    connections_.erase(connection->id_);
  }
} // namespace throttr