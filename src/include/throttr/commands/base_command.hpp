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

#ifndef THROTTR_COMMANDS_BASE_COMMAND_HPP
#define THROTTR_COMMANDS_BASE_COMMAND_HPP

#include <boost/asio/buffer.hpp>
#include <boost/uuid/uuid.hpp>
#include <memory>
#include <span>
#include <vector>

#include <throttr/protocol/request_types.hpp>

namespace throttr
{
  /**
   * Forward state
   */
  class state;

  /**
   * Base command
   */
  class base_command : public std::enable_shared_from_this<base_command>
  {
  public:
    /**
     * Call
     *
     * @param state
     * @param type
     * @param view
     * @param batch
     * @param write_buffer
     * @param id
     */
    static void call(
      const std::shared_ptr<state> &state,
      request_types type,
      std::span<const std::byte> view,
      std::vector<boost::asio::const_buffer> &batch,
      std::vector<std::byte> &write_buffer,
      boost::uuids::uuid id);
  };
} // namespace throttr

#endif // THROTTR_COMMANDS_BASE_COMMAND_HPP
