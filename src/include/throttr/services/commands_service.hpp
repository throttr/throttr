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

#include <throttr/commands/insert_command.hpp>
#include <throttr/commands/set_command.hpp>

#include <throttr/commands/query_command.hpp>
#include <throttr/commands/update_command.hpp>
#include <throttr/commands/purge_command.hpp>
#include <throttr/commands/list_command.hpp>
#include <throttr/commands/stat_command.hpp>
#include <throttr/commands/stats_command.hpp>
#include <throttr/commands/connections_command.hpp>

namespace throttr
{
  /**
   * Create service
   */
  class commands_service : public std::enable_shared_from_this<commands_service> {
  public:
    std::unordered_map<request_types, std::unique_ptr<base_command>> commands_;

    commands_service() {
      commands_[request_types::insert] = std::make_unique<insert_command>();
      commands_[request_types::set] = std::make_unique<set_command>();
      commands_[request_types::query] = std::make_unique<query_command>();
      commands_[request_types::get] = std::make_unique<query_command>();
      commands_[request_types::update] = std::make_unique<update_command>();
      commands_[request_types::purge] = std::make_unique<purge_command>();
      commands_[request_types::list] = std::make_unique<list_command>();
      commands_[request_types::stat] = std::make_unique<stat_command>();
      commands_[request_types::stats] = std::make_unique<stats_command>();
      commands_[request_types::connections] = std::make_unique<connections_command>();
    }
  };
} // namespace throttr

#endif // THROTTR_SERVICES_COMMANDS_SERVICE_HPP
