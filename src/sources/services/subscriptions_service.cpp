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

#include <mutex>
#include <throttr/services/subscriptions_service.hpp>

namespace throttr
{
  bool subscriptions_service::is_subscribed(const boost::uuids::uuid &id, const std::string_view channel) const
  {
    const auto &_index = subscriptions_.get<by_connection_id>();
    auto it = _index.find(id);
    while (it != _index.end())
    {
      if (it->channel_ == channel)
        return true;
      ++it;
    }
    return false;
  }
} // namespace throttr
