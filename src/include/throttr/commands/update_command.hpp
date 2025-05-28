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

#ifndef THROTTR_COMMANDS_UPDATE_COMMAND_HPP
#define THROTTR_COMMANDS_UPDATE_COMMAND_HPP

#include <boost/asio/buffer.hpp>
#include <memory>
#include <span>
#include <vector>

#include <throttr/commands/base_command.hpp>
#include <throttr/protocol/request_types.hpp>

namespace throttr
{
  /**
   * Update command
   */
  class update_command final : public base_command
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
     */
    void call(
      const std::shared_ptr<state> &state,
      request_types type,
      std::span<const std::byte> view,
      std::vector<boost::asio::const_buffer> &batch,
      std::vector<std::uint8_t> &write_buffer) override;
  };
} // namespace throttr

#endif // THROTTR_COMMANDS_UPDATE_COMMAND_HPP
