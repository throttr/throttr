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

#ifndef THROTTR_SERVICES_FIND_SERVICE_HPP
#define THROTTR_SERVICES_FIND_SERVICE_HPP

#include <memory>
#include <unordered_map>

#include <throttr/commands/base_command.hpp>
#include <throttr/protocol_wrapper.hpp>
#include <throttr/storage.hpp>

namespace throttr
{
  /**
   * Find service
   */
  class find_service : public std::enable_shared_from_this<find_service>
  {
  public:
    /**
     * Find or fail base
     *
     * @param state
     * @param key
     * @return
     */
    static std::optional<storage_iterator> find_or_fail(const std::shared_ptr<state> &state, const request_key &key);

    /**
     * Find or fail
     *
     * @param state
     * @param key
     * @param batch
     * @return
     */
    static std::optional<storage_iterator> find_or_fail_for_batch(
      const std::shared_ptr<state> &state,
      const request_key &key,
      std::vector<boost::asio::const_buffer> &batch);
  };
} // namespace throttr

#endif // THROTTR_SERVICES_FIND_SERVICE_HPP
