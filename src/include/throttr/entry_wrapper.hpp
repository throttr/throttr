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

#ifndef THROTTR_ENTRY_WRAPPER_HPP
#define THROTTR_ENTRY_WRAPPER_HPP

#include <memory>
#include <throttr/entry.hpp>
#include <throttr/entry_metrics.hpp>
#include <throttr/protocol_wrapper.hpp>
#include <vector>

namespace throttr
{
  /**
   * Entry wrapper
   */
  struct entry_wrapper
  {
    /**
     * Key
     */
    std::vector<std::byte> key_;

    /**
     * Entry
     */
    entry entry_;

    /**
     * Expired
     */
    bool expired_ = false;

#ifdef ENABLED_FEATURE_METRICS
    /**
     * Metrics
     */
    std::shared_ptr<entry_metrics> metrics_ = std::make_shared<entry_metrics>();
#endif

    /**
     * Key
     *
     * @return
     */
    [[nodiscard]] request_key key() const
    {
      const auto *raw = std::bit_cast<const char *>(key_.data());
      return {std::string_view(raw, key_.size())};
    }

    /**
     * Entry wrapper
     *
     * @param k
     * @param type
     * @param value
     * @param ttl_type
     * @param expires_at
     */
    entry_wrapper(
      std::vector<std::byte> k,
      const entry_types type,
      const std::span<const std::byte> value,
      const ttl_types ttl_type,
      const uint64_t expires_at) :
        key_(std::move(k)), entry_(type, value, ttl_type, expires_at)
    {
#ifdef ENABLED_FEATURE_METRICS
      metrics_->writes_.fetch_add(1, std::memory_order_relaxed);
#endif
    }

    entry_wrapper(const entry_wrapper &) = delete;
    entry_wrapper &operator=(const entry_wrapper &) = delete;

    entry_wrapper(entry_wrapper &&) = default;
    entry_wrapper &operator=(entry_wrapper &&) = default;
  };
} // namespace throttr

#endif // THROTTR_ENTRY_WRAPPER_HPP
