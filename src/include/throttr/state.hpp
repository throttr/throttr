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

#include <fmt/chrono.h>

#include <atomic>
#include <boost/asio/strand.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/core/ignore_unused.hpp>
#include <cstring>
#include <deque>
#include <memory>
#include <throttr/protocol_wrapper.hpp>
#include <throttr/storage.hpp>
#include <throttr/time.hpp>
#include <throttr/utils.hpp>
#include <vector>

namespace throttr
{
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
     * Constructor
     *
     * @param ioc
     */
    explicit state(boost::asio::io_context &ioc) :
      expiration_timer_(ioc),
#ifdef ENABLED_FEATURE_METRICS
      metrics_timer_(ioc),
#endif
      strand_(ioc.get_executor())
    {
    }

    /**
     * Handle create
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
      const ttl_types ttl_type,
      uint64_t ttl,
      const entry_types type,
      const bool as_insert = false)
    {
      const auto _now = std::chrono::steady_clock::now();
      const auto _expires_at = get_expiration_point(_now, ttl_type, ttl);

      auto &_index = storage_.get<tag_by_key>();
      const auto _key = std::vector<std::byte>(
        reinterpret_cast<const std::byte *>(key.data()), reinterpret_cast<const std::byte *>(key.data() + key.size())); // NOSONAR
      const auto _entry_ptr = entry_wrapper{_key, request_entry{type, value, ttl_type, _expires_at}};

#ifdef ENABLED_FEATURE_METRICS
      _entry_ptr.metrics_->stats_writes_.fetch_add(1, std::memory_order_relaxed);
#endif

      auto [_it, _inserted] = storage_.insert(std::move(_entry_ptr));

      boost::ignore_unused(_it);

      // LCOV_EXCL_START Note: Actually tested
      if (_inserted)
      {
        // OPTIMIZATION AVAILABLE
        // Optimize this as the current scheduled key is stored as member of this class
        // This will reduce the number of reads operations at least to one instead of all the container
        // Basically this operation is costly when a huge amount of keys are stored
        for (const auto &_item : _index)
        {
          _item.metrics_->stats_reads_.fetch_add(1, std::memory_order_relaxed);
          if (!_item.expired_)
          {
            if (_expires_at <= _item.entry_.expires_at_)
            {
              boost::asio::post(strand_, [self = shared_from_this(), _expires_at] { self->schedule_expiration(_expires_at); });
            }
            break;
          }
        }
      }
      // LCOV_EXCL_STOP

      // LCOV_EXCL_START
#ifndef NDEBUG
      fmt::println(
        "{:%Y-%m-%d %H:%M:%S} REQUEST {} key={} value={}ttl_type={} ttl={} "
        "RESPONSE ok={}",
        std::chrono::system_clock::now(),
        as_insert ? "INSERT" : "SET",
        key,
        span_to_hex(std::span(_entry_ptr.entry_.value_.data(), _entry_ptr.entry_.value_.size())),
        to_string(ttl_type),
        ttl,
        _inserted);
#endif
      // LCOV_EXCL_STOP

      return _inserted ? success_response_ : failed_response_;
    }

    /**
     * Handle insert
     *
     * @param view
     * @return uint8_t
     */
    std::uint8_t handle_insert(const std::span<const std::byte> view)
    {
      const auto _request = request_insert::from_buffer(view);
      std::vector<std::byte> value(sizeof(value_type));
      std::memcpy(value.data(), &_request.header_->quota_, sizeof(value_type));
      return handle_create(_request.key_, value, _request.header_->ttl_type_, _request.header_->ttl_, entry_types::counter);
    }

    /**
     * Handle set
     *
     * @param view
     * @return uint8_t
     */
    std::uint8_t handle_set(const std::span<const std::byte> view)
    {
      const auto _request = request_set::from_buffer(view);
      const std::vector value(_request.value_.begin(), _request.value_.end());
      return handle_create(_request.key_, value, _request.header_->ttl_type_, _request.header_->ttl_, entry_types::raw);
    }

    /**
     * Find or fail base
     *
     * @param key
     * @return
     */
    std::optional<storage_iterator> find_or_fail(const request_key & key) {
      const auto &_index = storage_.get<tag_by_key>();
      auto _it = _index.find(key);
      if (_it == _index.end() || _it->expired_) // LCOV_EXCL_LINE
      {
        return std::nullopt;
      }

#ifdef ENABLED_FEATURE_METRICS
      _it->metrics_->stats_reads_.fetch_add(1, std::memory_order_relaxed);
#endif

      return _it;
    }

    /**
     * Find or fail
     *
     * @param key
     * @param batch
     * @return
     */
    std::optional<storage_iterator> find_or_fail_for_batch(const request_key & key, std::vector<boost::asio::const_buffer> &batch) {
      const auto _it = find_or_fail(key);
      if (!_it.has_value()) // LCOV_EXCL_LINE
      {
        batch.emplace_back(boost::asio::buffer(&failed_response_, 1));
      }
      return _it;
    }

    /**
     * Handle query
     *
     * @param request
     * @param as_query
     * @param write_buffer
     * @param batch
     * @return uint8_t
     */
    void handle_query(
      const request_query &request,
      const bool as_query,
      std::vector<boost::asio::const_buffer> &batch,
      std::vector<std::uint8_t> &write_buffer)
    {
      const request_key _key{request.key_};
      const auto _find = find_or_fail_for_batch(_key, batch);
      if (!_find.has_value()) { // LCOV_EXCL_LINE Note: Partially tested
        // LCOV_EXCL_START
#ifndef NDEBUG
        fmt::println(
          "{:%Y-%m-%d %H:%M:%S} REQUEST {} key={} RESPONSE ok=false",
          std::chrono::system_clock::now(),
          as_query ? "QUERY" : "GET",
          _key.key_);
#endif
        // LCOV_EXCL_STOP
        return;
      }
      const auto _it = _find.value();
      const value_type _ttl = get_ttl(_it->entry_.expires_at_, _it->entry_.ttl_type_);

      // status_
      batch.emplace_back(boost::asio::buffer(&success_response_, 1));

      if (as_query) // LCOV_EXCL_LINE
      {
        // Value
        batch.push_back(boost::asio::buffer(_it->entry_.value_.data(), sizeof(value_type)));
        // TTL Type
        batch.push_back(boost::asio::buffer(&_it->entry_.ttl_type_, sizeof(_it->entry_.ttl_type_)));
        // TTL
        {
          const auto _offset = write_buffer.size();
          const auto *_ttl_ptr = reinterpret_cast<const std::uint8_t *>(&_ttl); // NOSONAR
          write_buffer.insert(write_buffer.end(), _ttl_ptr, _ttl_ptr + sizeof(_ttl));
          batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(_ttl)));
        }
      }
      else
      {
        // TTL Type
        batch.emplace_back(boost::asio::buffer(&_it->entry_.ttl_type_, sizeof(_it->entry_.ttl_type_)));
        // TTL
        {
          const auto _offset = write_buffer.size();
          const auto *_ttl_ptr = reinterpret_cast<const std::uint8_t *>(&_ttl); // NOSONAR
          write_buffer.insert(write_buffer.end(), _ttl_ptr, _ttl_ptr + sizeof(_ttl));
          batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(_ttl)));
        }
        // Size
        {
          const auto _offset = write_buffer.size();
          const auto _size = static_cast<value_type>(_it->entry_.value_.size());
          const auto *_size_ptr = reinterpret_cast<const std::uint8_t *>(&_size); // NOSONAR
          write_buffer.insert(write_buffer.end(), _size_ptr, _size_ptr + sizeof(_size));
          batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(_size)));
        }
        // Value
        batch.emplace_back(boost::asio::buffer(_it->entry_.value_.data(), _it->entry_.value_.size()));
      }

      // LCOV_EXCL_START
#ifndef NDEBUG
      if (as_query)
      {
        auto *_quota = reinterpret_cast<const value_type *>(_it->entry_.value_.data()); // NOSONAR
        fmt::println(
          "{:%Y-%m-%d %H:%M:%S} REQUEST QUERY key={} RESPONSE ok=true quota={} "
          "ttl_type={} ttl={}",
          std::chrono::system_clock::now(),
          _key.key_,
          *_quota,
          to_string(_it->entry_.ttl_type_),
          _ttl);
      }
      else
      {
        fmt::println(
          "{:%Y-%m-%d %H:%M:%S} REQUEST GET key={} RESPONSE ok=true value={} "
          "ttl_type={} ttl={}",
          std::chrono::system_clock::now(),
          _key.key_,
          span_to_hex(std::span(_it->entry_.value_.data(), _it->entry_.value_.size())),
          to_string(_it->entry_.ttl_type_),
          _ttl);
      }
#endif
      // LCOV_EXCL_STOP
    }

    /**
     * Handle update
     *
     * @param request
     * @return uint8_t
     */
    std::uint8_t handle_update(const request_update &request)
    {
      const request_key _key{request.key_};
      const auto _now = std::chrono::steady_clock::now();
      const auto _it = find_or_fail(_key);

      if (!_it.has_value()) // LCOV_EXCL_LINE note: Partially covered.
      {
        return failed_response_;
      }

      using enum attribute_types;

      bool _modified = false;

      storage_.modify(
        _it.value(),
        [&](entry_wrapper &object)
        {
          switch (request.header_->attribute_)
          {
            case quota:
              if (object.entry_.type_ == entry_types::counter)
              { // LCOV_EXCL_LINE Note: Partially tested
                _modified = apply_quota_change(object.entry_, request);
              }
              break;
            case ttl:
              _modified = apply_ttl_change(object.entry_, request, _now, object.key_);
              break;
          }
        });

#ifdef ENABLED_FEATURE_METRICS
      if (_modified) { // LCOV_EXCL_LINE Note: Partially tested
        _it.value()->metrics_->stats_writes_.fetch_add(1, std::memory_order_relaxed);
      }
#endif

      // LCOV_EXCL_START
#ifndef NDEBUG
      fmt::println(
        "{:%Y-%m-%d %H:%M:%S} REQUEST UPDATE key={} attribute={} change={} "
        "value={} RESPONSE ok={}",
        std::chrono::system_clock::now(),
        _key.key_,
        to_string(request.header_->attribute_),
        to_string(request.header_->change_),
        request.header_->value_,
        _modified);
#endif
      // LCOV_EXCL_STOP

      return _modified ? success_response_ : failed_response_;
    }

    /**
     * Apply quota change
     *
     * @param entry
     * @param request
     * @return bool
     */
    static bool apply_quota_change(request_entry &entry, const request_update &request)
    {
      using enum change_types;
      auto &_atomic = *reinterpret_cast<std::atomic<value_type> *>(entry.value_.data()); // NOSONAR

      switch (request.header_->change_)
      {
        case patch:
          _atomic.store(request.header_->value_);
          break;
        case increase:
          _atomic.fetch_add(request.header_->value_, std::memory_order_relaxed);
          break;
        case decrease:
          if (_atomic.load(std::memory_order_relaxed) >= request.header_->value_) // LCOV_EXCL_LINE note: Partially covered.
          {
            _atomic.fetch_sub(request.header_->value_, std::memory_order_relaxed);
          }
          else
          {
            return false;
          }
          break;
      }
      return true;
    }

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
      std::span<const std::byte> key)
    {
      using enum ttl_types;
      std::chrono::nanoseconds _duration;

      switch (entry.ttl_type_)
      {
        case seconds:
          _duration = std::chrono::seconds(request.header_->value_);
          break;
        case milliseconds:
          _duration = std::chrono::milliseconds(request.header_->value_);
          break;
        case nanoseconds:
        default:
          _duration = std::chrono::nanoseconds(request.header_->value_);
          break;
      }

      switch (request.header_->change_)
      {
        case change_types::patch:
          entry.expires_at_ = now + _duration;
          break;
        case change_types::increase:
          entry.expires_at_ += _duration;
          break;
        case change_types::decrease:
          entry.expires_at_ -= _duration;
          break;
      }

      // LCOV_EXCL_START
      if (scheduled_key_.size() == key.size() && std::equal(scheduled_key_.begin(), scheduled_key_.end(), key.begin()))
      {
        boost::asio::post(
          strand_, [_self = shared_from_this(), _expires_at = entry.expires_at_] { _self->schedule_expiration(_expires_at); });
      }
      // LCOV_EXCL_STOP

      return true;
    }

    /**
     * Handle purge
     *
     * @param request
     * @return uint8_t
     */
    std::uint8_t handle_purge(const request_purge &request)
    {
      const request_key _key{request.key_};

      auto &_index = storage_.get<tag_by_key>();
      const auto _it = _index.find(_key);

      bool _erased = true;

      if (_it == _index.end() || _it->expired_) // LCOV_EXCL_LINE note: Partially covered.
      {
        _erased = false;
      }

#ifdef ENABLED_FEATURE_METRICS
      if (_it != _index.end()) { // LCOV_EXCL_LINE Note: Partially tested
        _it->metrics_->stats_reads_.fetch_add(1, std::memory_order_relaxed);
      }
#endif

      // LCOV_EXCL_START
#ifndef NDEBUG
      fmt::
        println("{:%Y-%m-%d %H:%M:%S} REQUEST PURGE key={} RESPONSE ok={}", std::chrono::system_clock::now(), _key.key_, _erased);
#endif
      // LCOV_EXCL_STOP

      if (_erased) // LCOV_EXCL_LINE Note: Partially tested.
        _index.erase(_it);

      return _erased ? success_response_ : failed_response_;
    }

    /**
     * Handle list
     *
     * @param write_buffer
     * @param batch
     * @return uint8_t
     */
    void handle_list(std::vector<boost::asio::const_buffer> &batch, std::vector<std::uint8_t> &write_buffer)
    {
      std::size_t _fragments_count = 1;
      std::size_t _fragment_size = 0;
      constexpr std::size_t _max_fragment_size = 2048;

      std::vector<const entry_wrapper *> _fragment_items;
      std::vector<std::vector<const entry_wrapper *>> _fragments;

      const auto &_index = storage_.get<tag_by_key>();

      for (auto &_item : _index) // LCOV_EXCL_LINE Partially tested
      {
        const std::size_t _item_size = _item.key_.size() + _item.entry_.value_.size() + 11;
#ifdef ENABLED_FEATURE_METRICS
        _item.metrics_->stats_reads_.fetch_add(1, std::memory_order_relaxed);
#endif

        if (_fragment_size + _item_size > _max_fragment_size) // LCOV_EXCL_LINE Partially tested
        {
          _fragments.push_back(_fragment_items);
          _fragment_size = 0;
          _fragments_count++;
          _fragment_items.clear();
        }

        _fragment_items.emplace_back(&_item);
        _fragment_size += _item_size;
      }

      if (!_fragment_items.empty()) // LCOV_EXCL_LINE Partially tested
      {
        _fragments.push_back(std::move(_fragment_items));
      }

      {
        const auto _offset = write_buffer.size();
        const uint64_t _fragment_count = _fragments.size();
        const auto *_count_ptr = reinterpret_cast<const std::uint8_t *>(&_fragment_count); // NOSONAR
        write_buffer.insert(write_buffer.end(), _count_ptr, _count_ptr + sizeof(_fragment_count));
        batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(_fragment_count)));
      }

      std::size_t _i = 0;
      for (const auto &_fragment : _fragments) // LCOV_EXCL_LINE Partially tested
      {
        const uint64_t _fragment_index = _i + 1;
        const uint64_t _key_count = _fragment.size();

        {
          const auto _offset = write_buffer.size();
          const auto *_fid_ptr = reinterpret_cast<const std::uint8_t *>(&_fragment_index); // NOSONAR
          write_buffer.insert(write_buffer.end(), _fid_ptr, _fid_ptr + sizeof(_fragment_index));
          batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(_fragment_index)));
        }

        {
          const auto _offset = write_buffer.size();
          const auto *_kcount_ptr = reinterpret_cast<const std::uint8_t *>(&_key_count); // NOSONAR
          write_buffer.insert(write_buffer.end(), _kcount_ptr, _kcount_ptr + sizeof(_key_count));
          batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(_key_count)));
        }

        for (const auto *_entry : _fragment) // LCOV_EXCL_LINE Partially tested
        {
          {
            const auto _offset = write_buffer.size();
            write_buffer.push_back(static_cast<std::uint8_t>(_entry->key_.size()));
            batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], 1));
          }

          batch.emplace_back(boost::asio::buffer(&_entry->entry_.type_, sizeof(_entry->entry_.type_)));
          batch.emplace_back(boost::asio::buffer(&_entry->entry_.ttl_type_, sizeof(_entry->entry_.ttl_type_)));

          {
            const auto _offset = write_buffer.size();
            const uint64_t _expired_at =
              std::chrono::duration_cast<std::chrono::nanoseconds>(_entry->entry_.expires_at_.time_since_epoch()).count();
            const auto *_exp_ptr = reinterpret_cast<const std::uint8_t *>(&_expired_at); // NOSONAR
            write_buffer.insert(write_buffer.end(), _exp_ptr, _exp_ptr + sizeof(uint64_t));
            batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(uint64_t)));
          }

          {
            const auto _offset = write_buffer.size();
            value_type _bytes_used = static_cast<value_type>(_entry->entry_.value_.size());
            const auto *_used_ptr = reinterpret_cast<const std::uint8_t *>(&_bytes_used); // NOSONAR
            write_buffer.insert(write_buffer.end(), _used_ptr, _used_ptr + sizeof(value_type));
            batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(value_type)));
          }
        }

        for (const auto &_item : _fragment) // LCOV_EXCL_LINE Partially tested
        {
          batch.emplace_back(boost::asio::const_buffer(_item->key_.data(), _item->key_.size()));
        }
        _i++;
      }
    }

    /**
     * Handle stat
     *
     * @param request
     * @param write_buffer
     * @param batch
     * @return uint8_t
     */
    void handle_stat(
      const request_stat &request,
      std::vector<boost::asio::const_buffer> &batch,
      std::vector<std::uint8_t> &write_buffer)
    {
#ifndef ENABLED_FEATURE_METRICS
      batch.emplace_back(boost::asio::buffer(&failed_response_, 1));
      return;
#endif

      const request_key _key{request.key_};
      const auto _find = find_or_fail_for_batch(_key, batch);
      if (!_find.has_value()) // LCOV_EXCL_LINE
      {
        // LCOV_EXCL_START
#ifndef NDEBUG
        fmt::println(
          "{:%Y-%m-%d %H:%M:%S} REQUEST STAT key={} RESPONSE ok=false",
          std::chrono::system_clock::now(),
          _key.key_);
#endif
        return;
        // LCOV_EXCL_STOP
      }
      const auto _it = _find.value();
      batch.emplace_back(boost::asio::buffer(&success_response_, 1));

      auto _append_uint64 = [&write_buffer, &batch](const uint64_t value) {
        const auto _offset = write_buffer.size();
        const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&value); // NOSONAR
        write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(uint64_t));
        batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(uint64_t)));
      };

      const auto &metrics = *_it->metrics_;
      _append_uint64(metrics.stats_reads_per_minute_.load(std::memory_order_relaxed));
      _append_uint64(metrics.stats_writes_per_minute_.load(std::memory_order_relaxed));
      _append_uint64(metrics.stats_reads_accumulator_.load(std::memory_order_relaxed));
      _append_uint64(metrics.stats_writes_accumulator_.load(std::memory_order_relaxed));

      // LCOV_EXCL_START
#ifndef NDEBUG
      fmt::println(
        "{:%Y-%m-%d %H:%M:%S} REQUEST STAT key={} RESPONSE ok=true rpm={} wpm={} r_total={} w_total={}",
        std::chrono::system_clock::now(),
        _key.key_,
        metrics.stats_reads_per_minute_.load(),
        metrics.stats_writes_per_minute_.load(),
        metrics.stats_reads_accumulator_.load(),
        metrics.stats_writes_accumulator_.load());
#endif
      // LCOV_EXCL_STOP
    }

    /**
     * Expired entries
     */
    std::deque<std::pair<entry_wrapper, std::chrono::steady_clock::time_point>> expired_entries_;

    // LCOV_EXCL_START
    /**
     * Expiration timer
     */
    void expiration_timer()
    {
      std::scoped_lock guard(mutex_);
#ifndef NDEBUG
      fmt::println("{:%Y-%m-%d %H:%M:%S} GARBAGE COLLECTION STARTED", std::chrono::system_clock::now());
#endif

      const auto _now = std::chrono::steady_clock::now();
      auto &_index = storage_.get<tag_by_key>();

      std::vector<request_key> _to_expire;
      std::vector<request_key> _to_erase;
      std::chrono::steady_clock::time_point _next_expiration = std::chrono::steady_clock::time_point::max();
      for (const auto &_item : _index)
      {
        if (_item.expired_)
        {
          if (_now - _item.entry_.expires_at_ > std::chrono::seconds(10))
          {
            _to_erase.emplace_back(_item.key());
          }
          continue;
        }
        if (_item.entry_.expires_at_ <= _now)
        {
          _to_expire.emplace_back(_item.key());
        }
        else
        {
          _next_expiration = std::min(_next_expiration, _item.entry_.expires_at_);
        }
      }

      for (const auto &_key : _to_expire)
      {
        if (auto _it = _index.find(_key); _it != _index.end() && !_it->expired_)
        {
          _index.modify(
            _it,
            [](entry_wrapper &entry)
            {
#ifndef NDEBUG
              fmt::println(
                "{:%Y-%m-%d %H:%M:%S} GARBAGE COLLECTOR MARKED KEY AS EXPIRED key={}",
                std::chrono::system_clock::now(),
                std::string_view(
                  reinterpret_cast<const char *>(entry.key_.data()), // NOSONAR
                  entry.key_.size()));                               // NOSONAR
#endif
              entry.expired_ = true;
            });
        }
      }

      auto &_erase_index = storage_.get<tag_by_key>();
      for (const auto &_key : _to_erase)
      {
        if (auto _it = _erase_index.find(_key); _it != _erase_index.end() && _it->expired_)
        {
#ifndef NDEBUG
          fmt::println(
            "{:%Y-%m-%d %H:%M:%S} GARBAGE COLLECTOR ERASED EXPIRED KEY key={}",
            std::chrono::system_clock::now(),
            std::string_view(reinterpret_cast<const char *>(_it->key_.data()), _it->key_.size())); // NOSONAR
#endif
          _erase_index.erase(_it);
        }
      }

      for (auto &_candidate_index = storage_.get<tag_by_key>(); const auto &_item : _candidate_index)
      {
        const auto &_expires_at = _item.entry_.expires_at_;
        std::chrono::steady_clock::time_point _candidate;

        if (_item.expired_)
          _candidate = _expires_at + std::chrono::seconds(10);
        else
          _candidate = _expires_at;

        _next_expiration = std::min(_next_expiration, _candidate);
      }

#ifndef NDEBUG
      fmt::println("{:%Y-%m-%d %H:%M:%S} GARBAGE COLLECTION COMPLETED", std::chrono::system_clock::now());
#endif

      if (_next_expiration != std::chrono::steady_clock::time_point::max())
        schedule_expiration(_next_expiration);
    }

    /**
     * Schedule expiration
     */
    void schedule_expiration(const std::chrono::steady_clock::time_point proposed)
    {
#ifndef NDEBUG
      fmt::println("{:%Y-%m-%d %H:%M:%S} GARBAGE COLLECTION SCHEDULED", std::chrono::system_clock::now());
#endif

      const auto _now = std::chrono::steady_clock::now();
      if (proposed <= _now)
      {
        expiration_timer(); // Ejecutar inmediatamente si ya está vencido
        return;
      }

      const auto _delay = proposed - _now;
      expiration_timer_.expires_after(_delay);
      expiration_timer_.async_wait(
        [_self = shared_from_this()](const boost::system::error_code &ec)
        {
          if (!ec)
            _self->expiration_timer();
        });
    }

#ifdef ENABLED_FEATURE_METRICS
    void process_metrics()
    {
#ifndef NDEBUG
      fmt::println("{:%Y-%m-%d %H:%M:%S} METRICS SNAPSHOT STARTED", std::chrono::system_clock::now());
#endif
      for (auto &_index = storage_.get<tag_by_key>(); auto &_entry : _index)
      {
        if (_entry.expired_) continue;

        auto &_metric = *_entry.metrics_;
        const auto reads = _metric.stats_reads_.exchange(0, std::memory_order_relaxed);
        const auto writes = _metric.stats_writes_.exchange(0, std::memory_order_relaxed);

        _metric.stats_reads_per_minute_.store(reads, std::memory_order_relaxed);
        _metric.stats_writes_per_minute_.store(writes, std::memory_order_relaxed);

        _metric.stats_reads_accumulator_.fetch_add(reads, std::memory_order_relaxed);
        _metric.stats_writes_accumulator_.fetch_add(writes, std::memory_order_relaxed);
      }

#ifndef NDEBUG
      fmt::println("{:%Y-%m-%d %H:%M:%S} METRICS SNAPSHOT COMPLETED", std::chrono::system_clock::now());
#endif
    }

    void start_metrics_timer()
    {
#ifndef NDEBUG
      fmt::println("{:%Y-%m-%d %H:%M:%S} METRICS SNAPSHOT SCHEDULED", std::chrono::system_clock::now());
#endif

      metrics_timer_.expires_after(std::chrono::minutes(1));
      metrics_timer_.async_wait(
        boost::asio::bind_executor(
          strand_,
          [_self = shared_from_this()](const boost::system::error_code &ec)
          {
            if (!ec)
            {
              _self->process_metrics();
              _self->start_metrics_timer();  // reschedule
            }
          }));
    }
#endif

  };
} // namespace throttr
// LCOV_EXCL_STOP

#endif // THROTTR_STATE_HPP
