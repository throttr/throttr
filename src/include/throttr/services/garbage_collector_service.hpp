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

#ifndef THROTTR_SERVICES_GARBAGE_COLLECTOR_SERVICE_HPP
#define THROTTR_SERVICES_GARBAGE_COLLECTOR_SERVICE_HPP

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
   * Garbage collector service
   */
  class garbage_collector_service : public std::enable_shared_from_this<garbage_collector_service>
  {
  public:
    /**
     * Schedule timer
     *
     * @param state
     * @param proposed
     * @return
     */
    void schedule_timer(const std::shared_ptr<state> &state, std::chrono::steady_clock::time_point proposed);

    /**
     * Run
     *
     * @param state
     */
    void run(const std::shared_ptr<state> &state);
  };
} // namespace throttr

#endif // THROTTR_SERVICES_GARBAGE_COLLECTOR_SERVICE_HPP
