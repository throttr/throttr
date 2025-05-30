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

#ifndef THROTTR_SERVICES_SUBSCRIPTIONS_SERVICE_HPP
#define THROTTR_SERVICES_SUBSCRIPTIONS_SERVICE_HPP

#include <memory>

#include <throttr/commands/base_command.hpp>
#include <throttr/subscription.hpp>

#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index_container.hpp>

namespace throttr
{
  /**
   * By connection ID
   */
  struct by_connection_id
  {
  };

  /**
   * By channel name
   */
  struct by_channel_name
  {
  };

  /**
   * Subscription container
   */
  using subscription_container = boost::multi_index::multi_index_container<
    subscription,
    boost::multi_index::indexed_by<
      boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<by_connection_id>,
        boost::multi_index::member<subscription, std::array<std::byte, 16>, &subscription::connection_id_>>,
      boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<by_channel_name>,
        boost::multi_index::const_mem_fun<subscription, std::string_view, &subscription::channel>>>>;

  /**
   * Subscriptions service
   */
  class subscriptions_service : public std::enable_shared_from_this<subscriptions_service>
  {
  public:
    /**
     * Subscriptions
     */
    subscription_container subscriptions_;

    /**
     * Mutex
     */
    std::mutex mutex_;

    /**
     * Is subscribed
     *
     * @param id
     * @param channel
     * @return
     */
    bool is_subscribed(const std::array<std::byte, 16> &id, std::string_view channel) const;
  };
} // namespace throttr

#endif // THROTTR_SERVICES_SUBSCRIPTIONS_SERVICE_HPP
