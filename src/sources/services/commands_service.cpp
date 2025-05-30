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

#include <throttr/services/commands_service.hpp>

#include <throttr/commands/base_command.hpp>
#include <throttr/commands/connection_command.hpp>
#include <throttr/commands/connections_command.hpp>
#include <throttr/commands/insert_command.hpp>
#include <throttr/commands/list_command.hpp>
#include <throttr/commands/purge_command.hpp>
#include <throttr/commands/query_command.hpp>
#include <throttr/commands/set_command.hpp>
#include <throttr/commands/stat_command.hpp>
#include <throttr/commands/stats_command.hpp>
#include <throttr/commands/update_command.hpp>
#include <throttr/commands/whoami_command.hpp>

namespace throttr
{
  commands_service::commands_service()
  {
    commands_.fill(&base_command::call);
    commands_[static_cast<std::size_t>(request_types::insert)] = &insert_command::call;
    commands_[static_cast<std::size_t>(request_types::set)] = &set_command::call;
    commands_[static_cast<std::size_t>(request_types::query)] = &query_command::call;
    commands_[static_cast<std::size_t>(request_types::get)] = &query_command::call;
    commands_[static_cast<std::size_t>(request_types::update)] = &update_command::call;
    commands_[static_cast<std::size_t>(request_types::purge)] = &purge_command::call;
    commands_[static_cast<std::size_t>(request_types::list)] = &list_command::call;
    commands_[static_cast<std::size_t>(request_types::stat)] = &stat_command::call;
    commands_[static_cast<std::size_t>(request_types::stats)] = &stats_command::call;
    commands_[static_cast<std::size_t>(request_types::connections)] = &connections_command::call;
    commands_[static_cast<std::size_t>(request_types::connection)] = &connection_command::call;
    commands_[static_cast<std::size_t>(request_types::whoami)] = &whoami_command::call;
  }
} // namespace throttr
