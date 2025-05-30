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

#ifndef THROTTR_SERVICES_CREATE_SERVICE_HPP
#define THROTTR_SERVICES_CREATE_SERVICE_HPP

#include <memory>
#include <string_view>
#include <vector>

#include <throttr/protocol_wrapper.hpp>

namespace throttr
{
  /**
   * Forward state
   */
  class state;

  /**
   * Create service
   */
  class create_service
  {
  public:
    /**
     * Use
     *
     * @param state
     * @param key
     * @param value
     * @param ttl_type
     * @param ttl
     * @param type
     * @param id
     * @param as_insert
     * @return
     */
    static bool use(
      const std::shared_ptr<state> &state,
      const std::string_view &key,
      const std::vector<std::byte> &value,
      ttl_types ttl_type,
      uint64_t ttl,
      entry_types type,
      const std::array<std::byte, 16> &id,
      bool as_insert = false);
  };
} // namespace throttr

#endif // THROTTR_SERVICES_CREATE_SERVICE_HPP
