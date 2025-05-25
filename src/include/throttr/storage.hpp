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

#ifndef THROTTR_STORAGE_HPP
#define THROTTR_STORAGE_HPP

#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index_container.hpp>
#include <throttr/protocol_wrapper.hpp>

namespace throttr
{
#ifdef ENABLED_FEATURE_METRICS
  struct entry_metrics
  {
    /**
     * Stats read
     */
    std::atomic<uint64_t> stats_reads_ = 0;

    /**
     * Stats write
     */
    std::atomic<uint64_t> stats_writes_ = 0;

    /**
     * Stats read accumulator
     */
    std::atomic<uint64_t> stats_reads_accumulator_ = 0;

    /**
     * Stats write accumulator
     */
    std::atomic<uint64_t> stats_writes_accumulator_ = 0;

    /**
     * Stats read per minute (RPM)
     */
    std::atomic<uint64_t> stats_reads_per_minute_ = 0;

    /**
     * Stats write per minute (WPM)
     */
    std::atomic<uint64_t> stats_writes_per_minute_ = 0;
  };
#endif

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
    request_entry entry_;

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
      return {
        std::string_view(
          reinterpret_cast<const char *>(key_.data()), // NOSONAR
          key_.size()),                                // NOSONAR
      };
    }

    /**
     * Entry wrapper
     *
     * @param k
     * @param e
     */
    entry_wrapper(std::vector<std::byte> k, request_entry e) :
        key_(std::move(k)),
        entry_(std::move(e))
    {
    }
  };

  /**
   * Tag: access by key
   */
  struct tag_by_key
  {
  };

  /**
   * Multi-index container type for request storage
   */
  using storage_type = boost::multi_index::multi_index_container<
    entry_wrapper,
    boost::multi_index::indexed_by<boost::multi_index::hashed_unique<
      boost::multi_index::tag<tag_by_key>,
      boost::multi_index::const_mem_fun<entry_wrapper, request_key, &entry_wrapper::key>,
      request_key_hasher,
      std::equal_to<request_key>>>>;
} // namespace throttr

#endif // THROTTR_STORAGE_HPP
