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

#ifndef THROTTR_SERVICES_UPDATE_SERVICE_HPP
#define THROTTR_SERVICES_UPDATE_SERVICE_HPP

#include <memory>

#include <throttr/entry.hpp>
#include <throttr/protocol_wrapper.hpp>

namespace throttr
{
  /**
   * Forward state
   */
  class state;

  /**
   * Update service
   */
  class update_service
  {
  public:
    /**
     * Apply quota change
     *
     * @param state
     * @param entry
     * @param request
     * @return bool
     */
    static bool apply_quota_change(const std::shared_ptr<state> &state, entry &entry, const request_update &request);

    /**
     * Apply TTL change
     *
     * @param state
     * @param entry
     * @param request
     * @param now
     * @param key
     * @return bool
     */
    static bool apply_ttl_change(
      const std::shared_ptr<state> &state,
      entry &entry,
      const request_update &request,
      const std::chrono::steady_clock::time_point &now,
      std::span<const std::byte> key);
  };
} // namespace throttr

#endif // THROTTR_SERVICES_UPDATE_SERVICE_HPP
