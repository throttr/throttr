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

#ifndef THROTTR__ENTRY_HPP
#define THROTTR__ENTRY_HPP

#include <atomic>
#include <throttr/buffers_pool.hpp>
#include <throttr/protocol_wrapper.hpp>

namespace throttr
{

  /**
   * Request entry
   */
  struct entry
  {
    /**
     * Type
     */
    entry_types type_;

    /**
     * Counter
     */
    std::atomic<value_type> counter_;

    /**
     * Buffer
     */
    std::shared_ptr<reusable_buffer> buffer_storage_;

    /**
     * TTL type
     */
    ttl_types ttl_type_ = ttl_types::nanoseconds;

    /**
     * Expires at
     */
    std::atomic<uint64_t> expires_at_;

    /**
     * Move constructor
     * @param other
     */
    entry(entry &&other) noexcept :
        type_(other.type_),
        buffer_storage_(other.buffer_storage_),
        ttl_type_(other.ttl_type_),
        expires_at_(other.expires_at_.load(std::memory_order_relaxed))
    {
      counter_.store(other.counter_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    /**
     * Move assignment
     *
     * @param other
     * @return
     */
    entry &operator=(entry &&other) noexcept
    {
      if (this != &other)
      {
        type_ = other.type_;
        buffer_storage_ = other.buffer_storage_;
        ttl_type_ = other.ttl_type_;
        expires_at_ = other.expires_at_.load(std::memory_order_relaxed);
        counter_.store(other.counter_.load(std::memory_order_relaxed), std::memory_order_relaxed);
      }
      return *this;
    }

    /**
     * Copy constructor
     *
     * @param other
     */
    entry(const entry &other) :
        type_(other.type_),
        buffer_storage_(other.buffer_storage_),
        ttl_type_(other.ttl_type_),
        expires_at_(other.expires_at_.load(std::memory_order_relaxed))
    {
      counter_.store(other.counter_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    /**
     * Copy assignment
     *
     * @param other
     * @return
     */
    entry &operator=(const entry &other)
    {
      if (this != &other)
      {
        type_ = other.type_;
        buffer_storage_ = other.buffer_storage_;
        ttl_type_ = other.ttl_type_;
        expires_at_ = other.expires_at_.load(std::memory_order_relaxed);
        counter_.store(other.counter_.load(std::memory_order_relaxed), std::memory_order_relaxed);
      }
      return *this;
    }

    /**
     * Constructor
     */
    explicit entry(entry_types type, std::span<const std::byte> value, ttl_types ttl_type, uint64_t expires_at);

    /**
     * Destructor
     */
    ~entry();

    /**
     * Update buffer
     *
     * @param value
     */
    void update_buffer(std::span<const std::byte> value) const;
  };
} // namespace throttr

#endif // THROTTR__ENTRY_HPP
