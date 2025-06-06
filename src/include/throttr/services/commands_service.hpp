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

#ifndef THROTTR_SERVICES_COMMANDS_SERVICE_HPP
#define THROTTR_SERVICES_COMMANDS_SERVICE_HPP

#include <memory>
#include <unordered_map>

#include <boost/uuid/uuid.hpp>
#include <throttr/commands/base_command.hpp>

namespace throttr
{
  /**
   * Commands service
   */
  class commands_service : public std::enable_shared_from_this<commands_service>
  {
  public:
    using command_callback = void (*)(
      const std::shared_ptr<state> &,
      request_types,
      std::span<const std::byte>,
      std::vector<boost::asio::const_buffer> &,
      std::vector<std::byte> &,
      boost::uuids::uuid);

    /**
     * Commands
     */
    std::array<command_callback, 32> commands_{};

    /**
     * Constructor
     */
    commands_service();
  };
} // namespace throttr

#endif // THROTTR_SERVICES_COMMANDS_SERVICE_HPP
