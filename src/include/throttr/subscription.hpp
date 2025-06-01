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

#ifndef THROTTR_SUBSCRIPTION_HPP
#define THROTTR_SUBSCRIPTION_HPP

#include <boost/uuid/uuid.hpp>
#include <throttr/subscription_metrics.hpp>

namespace throttr
{
  /**
   * Subscription
   */
  struct subscription
  {
    /**
     * Connection ID
     */
    std::array<std::byte, 16> connection_id_;

    /**
     * Channel
     */
    std::string channel_;

    /**
     * Subscribed at
     */
    std::uint64_t subscribed_at_ = 0;

#ifdef ENABLED_FEATURE_METRICS
    subscription_metrics metrics_;
#endif

    /**
     * Constructor
     *
     * @param connection_id
     * @param channel
     */
    subscription(const std::array<std::byte, 16> connection_id, std::string channel) :
        connection_id_(connection_id), channel_(std::move(channel))
    {
      subscribed_at_ =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
  };
} // namespace throttr

#endif // THROTTR_SUBSCRIPTION_HPP
