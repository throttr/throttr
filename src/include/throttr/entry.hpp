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

#ifndef THROTTR_ENTRY_HPP
#define THROTTR_ENTRY_HPP

#include <throttr/protocol_wrapper.hpp>

namespace throttr
{
  /**
   * Value view
   */
  struct value_view
  {
    /**
     * Pointer
     */
    char *pointer_;

    /**
     * Size
     */
    std::size_t size_;
  };

  /**
   * Value owned
   */
  struct value_owned
  {
    /**
     * Data
     */
    std::unique_ptr<char[]> data_; // NOSONAR

    /**
     * Size
     */
    std::size_t size_;

    /**
     * Constructor
     */
    value_owned() : data_(nullptr), size_(0)
    {
    }

    /**
     * Constructor
     *
     * @param sz
     */
    explicit value_owned(const std::size_t sz) : data_(std::make_unique<char[]>(sz)), size_(sz)
    {
    }

    /**
     * Constructor
     *
     * @param src
     * @param sz
     */
    value_owned(const char *src, const std::size_t sz) : data_(std::make_unique<char[]>(sz)), size_(sz)
    {
      std::memcpy(data_.get(), src, sz);
    }

    /**
     * View accessor
     * @return
     */
    [[nodiscard]] value_view view() const
    {
      return {data_.get(), size_};
    }
  };

  /**
   * Entry
   */
  struct entry
  {
    /**
     * Type
     */
    entry_types type_;

    /**
     * Value
     */
    value_owned value_;

    /**
     * TTL type
     */
    ttl_types ttl_type_;

    /**
     * Expires at
     */
    std::chrono::steady_clock::time_point expires_at_;
  };
} // namespace throttr

#endif // THROTTR_ENTRY_HPP
