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

#include <atomic>
#include <boost/uuid/uuid.hpp>

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
    boost::uuids::uuid connection_id_;

    /**
     * Channel
     */
    std::vector<std::byte> channel_;

    /**
     * Connected at
     */
    std::uint64_t connected_at_ = 0;

#ifdef ENABLED_FEATURE_METRICS

    /**
     * Bytes read
     */
    std::atomic<uint64_t> bytes_read_{0};

    /**
     * Bytes write
     */
    std::atomic<uint64_t> bytes_write_{0};
#endif

    /**
     * Constructor
     *
     * @param connection_id
     * @param channel
     */
    subscription(const boost::uuids::uuid connection_id, const std::vector<std::byte> &channel) :
        connection_id_(connection_id), channel_(channel)
    {
      connected_at_ =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    /**
     * Channel
     *
     * @return
     */
    [[nodiscard]] std::string_view channel() const noexcept
    {
      return {reinterpret_cast<const char *>(channel_.data()), channel_.size()}; // NOSONAR
    }
  };
} // namespace throttr

#endif // THROTTR_SUBSCRIPTION_HPP
