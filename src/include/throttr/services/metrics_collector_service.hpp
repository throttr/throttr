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

#ifndef THROTTR_SERVICES_METRICS_COLLECTOR_SERVICE_HPP
#define THROTTR_SERVICES_METRICS_COLLECTOR_SERVICE_HPP

#include <memory>

namespace throttr
{
  /**
   * Forward state
   */
  class state;

#ifdef ENABLED_FEATURE_METRICS
  /**
   * Metrics collector service
   */
  class metrics_collector_service : public std::enable_shared_from_this<metrics_collector_service>
  {
  public:
    /**
     * Schedule timer
     *
     * @param state
     * @return
     */
    void schedule_timer(const std::shared_ptr<state> &state);

    /**
     * Run
     *
     * @param state
     */
    static void run(const std::shared_ptr<state> &state);
  };
#endif
} // namespace throttr

#endif // THROTTR_SERVICES_METRICS_COLLECTOR_SERVICE_HPP
