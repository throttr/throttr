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
     * Expiration timer
     */
    boost::asio::steady_timer expiration_timer_;

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
    explicit state(boost::asio::io_context &ioc) : expiration_timer_(ioc), strand_(ioc.get_executor())
    {
    }

    /**
     * Handle entry
     *
     * @param key
     * @param value
     * @param ttl_type
     * @param ttl
     * @param type
     * @param as_insert
     * @return
     */
    std::uint8_t handle_entry(
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
      const auto _key = std::vector<
        std::byte>(reinterpret_cast<const std::byte *>(key.data()), reinterpret_cast<const std::byte *>(key.data() + key.size()));
      const auto _entry_ptr = entry_wrapper{_key, request_entry{type, value, ttl_type, _expires_at}};

      auto [_it, _inserted] = storage_.insert(_entry_ptr);

      boost::ignore_unused(_it);

      // LCOV_EXCL_START Note: Actually tested
      if (_inserted)
      {
        for (const auto &_item : _index)
        {
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
      return handle_entry(_request.key_, value, _request.header_->ttl_type_, _request.header_->ttl_, entry_types::counter);
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
      return handle_entry(_request.key_, value, _request.header_->ttl_type_, _request.header_->ttl_, entry_types::raw);
    }

    /**
     * Handle query
     *
     * @param request
     * @param as_query
     * @param write_buffer
     * @param write_offset
     * @param batch
     * @return uint8_t
     */
    void handle_query(
      const request_query &request,
      const bool as_query,
      std::vector<boost::asio::const_buffer> &batch,
      std::uint8_t *write_buffer,
      std::size_t &write_offset)
    {
      const request_key _key{request.key_};
      const auto &_index = storage_.get<tag_by_key>();
      const auto _it = _index.find(_key);

      if (_it == _index.end() || _it->expired_) // LCOV_EXCL_LINE
      {
        static constexpr std::uint8_t status = 0x00;
        std::memcpy(write_buffer + write_offset, &status, sizeof(status));
        batch.push_back(boost::asio::buffer(&status, sizeof(status)));
        write_offset += sizeof(status);

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

      const value_type _ttl = get_ttl(_it->entry_.expires_at_, _it->entry_.ttl_type_);

      // status_
      static constexpr std::uint8_t status = 0x01;
      std::memcpy(write_buffer + write_offset, &status, sizeof(status));
      batch.push_back(boost::asio::buffer(&status, sizeof(status)));
      write_offset += sizeof(status);

      if (as_query) // LCOV_EXCL_LINE
      {
        batch.push_back(boost::asio::buffer(_it->entry_.value_.data(), _it->entry_.value_.size()));
        batch.push_back(boost::asio::buffer(&_it->entry_.ttl_type_, sizeof(_it->entry_.ttl_type_)));
        std::memcpy(write_buffer + write_offset, &_ttl, sizeof(_ttl));
        batch.push_back(boost::asio::buffer(write_buffer + write_offset, sizeof(_ttl)));
        write_offset += sizeof(_ttl);
      }
      else
      {
        batch.push_back(boost::asio::buffer(&_it->entry_.ttl_type_, sizeof(_it->entry_.ttl_type_)));
        std::memcpy(write_buffer + write_offset, &_ttl, sizeof(_ttl));
        batch.push_back(boost::asio::buffer(write_buffer + write_offset, sizeof(_ttl)));
        write_offset += sizeof(_ttl);
        const auto _size = static_cast<value_type>(_it->entry_.value_.size());
        std::memcpy(write_buffer + write_offset, &_size, sizeof(_size));
        batch.push_back(boost::asio::buffer(write_buffer + write_offset, sizeof(_size)));
        write_offset += sizeof(_size);
        batch.push_back(boost::asio::buffer(_it->entry_.value_.data(), _it->entry_.value_.size()));
      }

      // LCOV_EXCL_START
#ifndef NDEBUG
      if (as_query)
      {
        auto *_quota = reinterpret_cast<const value_type *>(_it->entry_.value_.data());
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

      auto &_index = storage_.get<tag_by_key>();
      const auto _it = _index.find(_key);

      if (_it == _index.end() || _it->expired_) // LCOV_EXCL_LINE note: Partially covered.
      {
        return failed_response_;
      }

      using enum attribute_types;

      bool _modified = false;

      storage_.modify(
        _it,
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

      auto *_quota = reinterpret_cast<value_type *>(entry.value_.data());

      switch (request.header_->change_)
      {
        case patch:
          *_quota = request.header_->value_;
          break;
        case increase:
          *_quota += request.header_->value_;
          break;
        case decrease:
          if (*_quota >= request.header_->value_) // LCOV_EXCL_LINE note: Partially covered.
          {
            *_quota -= request.header_->value_;
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
      if (
        scheduled_key_.size() == key.size() &&
        std::equal(scheduled_key_.begin(), scheduled_key_.end(), key.begin()))
      {
        boost::asio::post(strand_, [_self = shared_from_this(), _expires_at = entry.expires_at_] { _self->schedule_expiration(_expires_at); });
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
     * @param write_offset
     * @param batch
     * @return uint8_t
     */
    void handle_list(std::vector<boost::asio::const_buffer> &batch, std::uint8_t *write_buffer, std::size_t &write_offset)
    {
      std::size_t _fragments_count = 1;
      std::size_t _fragment_size = 0;
      constexpr std::size_t _max_fragment_size = 2048;

      std::vector<const entry_wrapper*> _fragment_items;
      std::vector<std::vector<const entry_wrapper*>> _fragments;

      const auto &_index = storage_.get<tag_by_key>();

      for (auto &_item : _index)
      {
        const std::size_t _item_size = _item.key_.size() + _item.entry_.value_.size() + 11;

        if (_fragment_size + _item_size > _max_fragment_size)
        {
          _fragments.push_back(_fragment_items);
          _fragment_size = 0;
          _fragments_count++;
          _fragment_items.clear();
        }

        _fragment_items.emplace_back(&_item);
        _fragment_size += _item_size;
      }

      if (!_fragment_items.empty())
      {
        _fragments.push_back(std::move(_fragment_items));
      }

      const uint64_t fragment_count = _fragments.size();
      std::memcpy(write_buffer + write_offset, &fragment_count, sizeof(fragment_count));
      batch.emplace_back(write_buffer + write_offset, sizeof(fragment_count));
      write_offset += sizeof(fragment_count);

      std::size_t i = 0;
      for (auto fragment : _fragments) {
        const uint64_t fragment_id = i + 1;
        const uint64_t key_count = fragment.size();

        std::memcpy(write_buffer + write_offset, &fragment_id, sizeof(fragment_id));
        batch.emplace_back(write_buffer + write_offset, sizeof(fragment_id));
        write_offset += sizeof(uint64_t);

        std::memcpy(write_buffer + write_offset, &key_count, sizeof(key_count));
        batch.emplace_back(write_buffer + write_offset, sizeof(key_count));
        write_offset += sizeof(uint64_t);

        for (const auto *entry : fragment)
        {
          write_buffer[write_offset] = static_cast<std::uint8_t>(entry->key_.size());
          batch.emplace_back(write_buffer + write_offset, 1);
          write_offset += 1;

          batch.emplace_back(&entry->entry_.type_, sizeof(entry->entry_.type_));
          batch.emplace_back(&entry->entry_.ttl_type_, sizeof(entry->entry_.ttl_type_));

          const uint64_t exp = std::chrono::duration_cast<std::chrono::nanoseconds>(
              entry->entry_.expires_at_.time_since_epoch()).count();
          std::memcpy(write_buffer + write_offset, &exp, sizeof(uint64_t));
          batch.emplace_back(write_buffer + write_offset, sizeof(uint64_t));
          write_offset += sizeof(uint64_t);

          value_type used = static_cast<value_type>(entry->entry_.value_.size());
          std::memcpy(write_buffer + write_offset, &used, sizeof(value_type));
          batch.emplace_back(write_buffer + write_offset, sizeof(value_type));
          write_offset += sizeof(value_type);
        }

        for (const auto &_item : fragment)
        {
          batch.emplace_back(boost::asio::const_buffer(_item->key_.data(), _item->key_.size()));
        }
        i++;
      }
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
                "{:%Y-%m-%d %H:%M:%S} SCHEDULER KEY EXPIRED key={}",
                std::chrono::system_clock::now(),
                std::string_view(
                  reinterpret_cast<const char *>(entry.key_.data()),
                  entry.key_.size())); // NOSONAR
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
            "{:%Y-%m-%d %H:%M:%S} SCHEDULER KEY ERASED key={}",
            std::chrono::system_clock::now(),
            std::string_view(reinterpret_cast<const char *>(_it->key_.data()), _it->key_.size()));
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

      if (_next_expiration != std::chrono::steady_clock::time_point::max())
        schedule_expiration(_next_expiration);
    }

    /**
     * Schedule expiration
     */
    void schedule_expiration(const std::chrono::steady_clock::time_point proposed)
    {
#ifndef NDEBUG
      fmt::println("{:%Y-%m-%d %H:%M:%S} SCHEDULER GARBAGE COLLECTION STARTED", std::chrono::system_clock::now());
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
  };
} // namespace throttr
// LCOV_EXCL_STOP

#endif // THROTTR_STATE_HPP
