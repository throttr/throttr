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

#pragma once

#ifndef THROTTR_MESSAGE_HPP
#define THROTTR_MESSAGE_HPP

#include <boost/asio/buffer.hpp>
#include <vector>

namespace throttr
{
  /**
   * Message
   */
  class message : public std::enable_shared_from_this<message>
  {
  public:
    /**
     * Write buffer
     */
    std::vector<std::byte> write_buffer_;

    /**
     * Buffers
     */
    std::vector<boost::asio::const_buffer> buffers_;

    /**
     * Recyclable
     */
    bool recyclable_ = true;

    /**
     * Used
     */
    bool used_ = false;

    /**
     * Constructor
     */
    message() = default;
  };

  /**
   * Available message pool
   */
  inline thread_local constexpr std::vector<std::shared_ptr<message>> available_message_pool_;

  /**
   * Used message pool
   */
  inline thread_local constexpr std::vector<std::shared_ptr<message>> used_message_pool_;
} // namespace throttr

#endif // THROTTR_MESSAGE_HPP
