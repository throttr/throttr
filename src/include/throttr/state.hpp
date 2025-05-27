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

#ifndef THROTTR_STATE_HPP
#define THROTTR_STATE_HPP

#include <atomic>
#include <boost/asio/buffer.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/uuid/random_generator.hpp>
#include <cstring>
#include <deque>
#include <memory>
#include <throttr/protocol_wrapper.hpp>
#include <throttr/storage.hpp>
#include <vector>

namespace throttr
{
  /**
   * Forward connection
   */
  class connection;

  /**
   * State
   */
  class state : public std::enable_shared_from_this<state>
  {
  public:
    /**
     * Acceptor ready
     */
    std::atomic_bool acceptor_ready_;

    /**
     * Port exposed
     */
    std::uint16_t exposed_port_;

    /**
     * Storage
     */
    storage_type storage_;

    /**
     * Storage iterator
     */
    using storage_iterator = decltype(storage_.get<tag_by_key>().begin());

    /**
     * Expiration timer
     */
    boost::asio::steady_timer expiration_timer_;

    /**
     * UUID generator
     */
    boost::uuids::random_generator id_generator_ = boost::uuids::random_generator();

    /**
     * Connections container
     */
    std::unordered_map<boost::uuids::uuid, connection *, boost::hash<boost::uuids::uuid>> connections_;

    /**
     * Connections mutex
     */
    std::mutex connections_mutex_;

#ifdef ENABLED_FEATURE_METRICS
    /**
     * Metrics timer
     */
    boost::asio::steady_timer metrics_timer_;
#endif

    /**
     * Strand
     */
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

    /**
     * Scheduled key
     */
    std::span<const std::byte> scheduled_key_;

    /**
     * Remove lock
     */
    std::mutex mutex_;

    /**
     * Success response
     */
    constexpr static uint8_t success_response_ = 0x01;

    /**
     * Failed response
     */
    constexpr static uint8_t failed_response_ = 0x00;

    /**
     * Expired entries
     */
    std::deque<std::pair<entry_wrapper, std::chrono::steady_clock::time_point>> expired_entries_;

    /**
     * Constructor
     *
     * @param ioc
     */
    explicit state(boost::asio::io_context &ioc);

    /**
     * Handle CREATE
     *
     * @param key
     * @param value
     * @param ttl_type
     * @param ttl
     * @param type
     * @param as_insert
     * @return
     */
    std::uint8_t handle_create(
      const std::string_view &key,
      const std::vector<std::byte> &value,
      ttl_types ttl_type,
      uint64_t ttl,
      entry_types type,
      bool as_insert = false);

    /**
     * Handle INSERT
     *
     * @param view
     * @return uint8_t
     */
    std::uint8_t handle_insert(std::span<const std::byte> view);

    /**
     * Handle set
     *
     * @param view
     * @return uint8_t
     */
    std::uint8_t handle_set(std::span<const std::byte> view);

    /**
     * Find or fail base
     *
     * @param key
     * @return
     */
    std::optional<storage_iterator> find_or_fail(const request_key &key);

    /**
     * Find or fail
     *
     * @param key
     * @param batch
     * @return
     */
    std::optional<storage_iterator> find_or_fail_for_batch(const request_key &key, std::vector<boost::asio::const_buffer> &batch);

    /**
     * Handle QUERY
     *
     * @param request
     * @param as_query
     * @param write_buffer
     * @param batch
     * @return uint8_t
     */
    void handle_query(
      const request_query &request,
      bool as_query,
      std::vector<boost::asio::const_buffer> &batch,
      std::vector<std::uint8_t> &write_buffer);

    /**
     * Handle UPDATE
     *
     * @param request
     * @return uint8_t
     */
    std::uint8_t handle_update(const request_update &request);

    /**
     * Apply quota change
     *
     * @param entry
     * @param request
     * @return bool
     */
    static bool apply_quota_change(request_entry &entry, const request_update &request);

    /**
     * Apply TTL change
     *
     * @param entry
     * @param request
     * @param now
     * @param key
     * @return bool
     */
    bool apply_ttl_change(
      request_entry &entry,
      const request_update &request,
      const std::chrono::steady_clock::time_point &now,
      std::span<const std::byte> key);

    /**
     * Handle PURGE
     *
     * @param request
     * @return uint8_t
     */
    std::uint8_t handle_purge(const request_purge &request);

    /**
     * Write LIST entry to buffer
     *
     * @param batch
     * @param entry
     * @param write_buffer
     * @param measure
     * @return
     */
    static std::size_t write_list_entry_to_buffer(
      std::vector<boost::asio::const_buffer> *batch,
      const entry_wrapper *entry,
      std::vector<std::uint8_t> &write_buffer,
      bool measure);

    /**
     * Handle LIST
     *
     * @param write_buffer
     * @param batch
     * @return uint8_t
     */
    void handle_list(std::vector<boost::asio::const_buffer> &batch, std::vector<std::uint8_t> &write_buffer);

    /**
     * Handle STAT
     *
     * @param request
     * @param write_buffer
     * @param batch
     * @return uint8_t
     */
    void handle_stat(
      const request_stat &request,
      std::vector<boost::asio::const_buffer> &batch,
      std::vector<std::uint8_t> &write_buffer);

    /**
     * Handle CONNECTIONS
     *
     * @param write_buffer
     * @param batch
     * @return uint8_t
     */
    void handle_connections(std::vector<boost::asio::const_buffer> &batch, std::vector<std::uint8_t> &write_buffer);

    /**
     * Write STATS entry to buffer
     *
     * @param batch
     * @param entry
     * @param write_buffer
     * @param measure
     * @return
     */
    static std::size_t write_stats_entry_to_buffer(
      std::vector<boost::asio::const_buffer> *batch,
      const entry_wrapper *entry,
      std::vector<std::uint8_t> &write_buffer,
      bool measure);

    /**
     * Write CONNECTIONS entry to buffer
     *
     * @param batch
     * @param conn
     * @param write_buffer
     * @param measure
     * @return
     */
    static std::size_t write_connections_entry_to_buffer(
      std::vector<boost::asio::const_buffer> *batch,
      const connection *conn,
      std::vector<std::uint8_t> &write_buffer,
      bool measure);

    /**
     * Handle STATS
     *
     * @param batch
     * @param write_buffer
     */
    void handle_stats(std::vector<boost::asio::const_buffer> &batch, std::vector<std::uint8_t> &write_buffer);

    /**
     * Handle fragmented connections response
     *
     * @param batch
     * @param write_buffer
     */
    void handle_fragmented_connections_response(
      std::vector<boost::asio::const_buffer> &batch,
      std::vector<std::uint8_t> &write_buffer);

    /**
     * Handle fragmented entries response
     *
     * @tparam EntrySerializer
     * @param batch
     * @param write_buffer
     * @param max_fragment_size
     * @param serialize_entry
     */
    template<typename EntrySerializer>
    void handle_fragmented_entries_response(
      std::vector<boost::asio::const_buffer> &batch,
      std::vector<std::uint8_t> &write_buffer,
      const std::size_t max_fragment_size,
      EntrySerializer &&serialize_entry)
    {
      const auto &_index = storage_.get<tag_by_key>();
      std::size_t _fragments_count = 1;
      std::size_t _fragment_size = 0;

      std::vector<const entry_wrapper *> _fragment_items;
      std::vector<std::vector<const entry_wrapper *>> _fragments;

      for (auto &_item : _index) // LCOV_EXCL_LINE Note: Partially tested.
      {
        // LCOV_EXCL_START
        if (_item.expired_)
          continue;
          // LCOV_EXCL_STOP

#ifdef ENABLED_FEATURE_METRICS
        _item.metrics_->reads_.fetch_add(1, std::memory_order_relaxed);
#endif

        const std::size_t _item_size = serialize_entry(nullptr, &_item, true);
        if (_fragment_size + _item_size > max_fragment_size) // LCOV_EXCL_LINE Note: Partially tested.
        {
          _fragments.push_back(_fragment_items);
          _fragment_size = 0;
          _fragments_count++;
          _fragment_items.clear();
        }

        _fragment_items.emplace_back(&_item);
        _fragment_size += _item_size;
      }

      if (!_fragment_items.empty()) // LCOV_EXCL_LINE Note: Partially tested.
      {
        _fragments.push_back(std::move(_fragment_items));
      }

      {
        const auto _offset = write_buffer.size();
        const uint64_t _fragment_count = _fragments.size();
        const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&_fragment_count); // NOSONAR
        write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(_fragment_count));
        batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(_fragment_count)));
      }

      std::size_t _i = 0;
      for (const auto &_fragment : _fragments) // LCOV_EXCL_LINE Note: Partially tested.
      {
        const uint64_t _fragment_index = _i + 1;
        const uint64_t _key_count = _fragment.size();

        for (uint64_t value : {_fragment_index, _key_count}) // LCOV_EXCL_LINE Note: Partially tested.
        {
          const auto _offset = write_buffer.size();
          const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&value); // NOSONAR
          write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(value));
          batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(value)));
        }

        for (const auto *_entry : _fragment) // LCOV_EXCL_LINE Note: Partially tested.
        {
          serialize_entry(&batch, _entry, false);
        }

        for (const auto &_entry : _fragment) // LCOV_EXCL_LINE Note: Partially tested.
        {
          batch.emplace_back(boost::asio::buffer(_entry->key_.data(), _entry->key_.size()));
        }

        _i++;
      }
    }

    /**
     * Join
     *
     * @param connection
     */
    void join(connection *connection);

    /**
     * Leave
     *
     * @param connection
     */
    void leave(const connection *connection);

    /**
     * Expiration timer
     */
    void expiration_timer();

    /**
     * Schedule expiration
     */
    void schedule_expiration(std::chrono::steady_clock::time_point proposed);

#ifdef ENABLED_FEATURE_METRICS
    /**
     * Process metrics
     */
    void process_metrics();

    /**
     * Start metrics timer
     */
    void start_metrics_timer();
#endif
  };
} // namespace throttr

#endif // THROTTR_STATE_HPP
