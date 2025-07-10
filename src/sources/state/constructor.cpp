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

#include <throttr/services/find_service.hpp>
#include <throttr/services/garbage_collector_service.hpp>
#include <throttr/services/response_builder_service.hpp>
#include <throttr/services/subscriptions_service.hpp>
#include <throttr/state.hpp>

namespace throttr
{
  state::state(boost::asio::io_context &ioc) :
      expiration_timer_(ioc),
#ifdef ENABLED_FEATURE_METRICS
      metrics_timer_(ioc),
#endif
      strand_(ioc.get_executor()),
      commands_(std::make_shared<commands_service>()),
      subscriptions_(std::make_shared<subscriptions_service>()),
      messages_(std::make_shared<messages_service>()),
      finder_(std::make_shared<find_service>()),
      response_builder_(std::make_shared<response_builder_service>()),
      garbage_collector_(std::make_shared<garbage_collector_service>())
#ifdef ENABLED_FEATURE_METRICS
      ,
      metrics_collector_(std::make_shared<metrics_collector_service>())
#endif
  {
    started_at_ =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  }
} // namespace throttr