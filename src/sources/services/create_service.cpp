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

#include "throttr/connection_allocator.hpp"

#include <boost/asio/bind_allocator.hpp>
#include <throttr/services/create_service.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <throttr/entry.hpp>
#include <throttr/services/garbage_collector_service.hpp>
#include <throttr/state.hpp>
#include <throttr/time.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  bool create_service::use(
    const std::shared_ptr<state> &state,
    std::span<const std::byte> key,
    std::span<const std::byte> value,
    const ttl_types ttl_type,
    std::span<const std::byte> ttl,
    const entry_types type,
    const boost::uuids::uuid &id,
    const bool as_insert)
  {
    boost::ignore_unused(as_insert, id);

    using namespace boost::endian;

    const auto _now = std::chrono::system_clock::now();
    const auto _now_ns = _now.time_since_epoch().count();
    const auto _expires_at = get_expiration_point(_now_ns, ttl_type, ttl);

    auto &_index = state->storage_.get<tag_by_key>();
    const std::vector _key(key.data(), key.data() + key.size());

    if (!as_insert)
    {
      std::string _scoped_key(key.size(), '\0');
      std::memcpy(_scoped_key.data(), key.data(), key.size());

      const request_key _lookup_key{_scoped_key};
      if (const auto _it_existing = _index.find(_lookup_key);
          _it_existing != _index.end() && _it_existing->entry_.type_ != entry_types::counter)
      {
        auto _modified = _index.modify(
          _it_existing,
          [value, ttl_type, _expires_at](entry_wrapper &item)
          {
            while (state::available_buffers_.size() < 64)
            {
              auto _scoped_buff = std::make_shared<std::vector<std::byte>>(std::numeric_limits<value_type>::max());
              state::available_buffers_.push_back(std::move(_scoped_buff));
            }

            const auto _buffer = state::available_buffers_.front();
            state::available_buffers_.erase(state::available_buffers_.begin());

            _buffer->resize(value.size());
            std::memcpy(_buffer->data(), value.data(), value.size());

            item.entry_.buffer_.store(_buffer, std::memory_order_release);
            item.entry_.expires_at_ = _expires_at;
            item.entry_.ttl_type_ = ttl_type;

#ifdef ENABLED_FEATURE_METRICS
            item.metrics_->writes_.fetch_add(1, std::memory_order_relaxed);
#endif
          });

#ifndef NDEBUG
        fmt::println(
          "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST SET AGAIN session_id={} META key={} value={}ttl_type={} ttl={} RESPONSE ok=true"
          "RESPONSE ok={}",
          to_string(state->id_),
          std::chrono::system_clock::now(),
          to_string(id),
          span_to_hex(key),
          span_to_hex(value),
          to_string(ttl_type),
          span_to_hex(ttl),
          _modified);
#endif
        return _modified;
      }
    }

    entry _entry;
    _entry.type_ = type;
    _entry.ttl_type_ = ttl_type;
    _entry.expires_at_ = _expires_at;

    if (_entry.type_ == entry_types::counter)
    {
      value_type _parsed_value = 0;
      std::memcpy(&_parsed_value, value.data(), sizeof(value_type));
      _entry.counter_.store(little_to_native(_parsed_value), std::memory_order_relaxed);
    }
    else
    {
      while (state::available_buffers_.size() < 64)
      {
        auto _scoped_buff = std::make_shared<std::vector<std::byte>>(std::numeric_limits<value_type>::max());
        state::available_buffers_.push_back(std::move(_scoped_buff));
      }

      const auto _buffer = state::available_buffers_.front();
      state::available_buffers_.erase(state::available_buffers_.begin());

      _buffer->resize(value.size());
      std::memcpy(_buffer->data(), value.data(), value.size());

      _entry.buffer_.store(_buffer, std::memory_order_release);
    }

    const auto _entry_ptr = entry_wrapper{_key, _entry};

#ifdef ENABLED_FEATURE_METRICS
    _entry_ptr.metrics_->writes_.fetch_add(1, std::memory_order_relaxed);
#endif

    auto [_it, _inserted] = state->storage_.insert(_entry_ptr);

    boost::ignore_unused(_it);

    if (_inserted)
    {
      // OPTIMIZATION AVAILABLE
      // Optimize this as the current scheduled key is stored as member of this class
      // This will reduce the number of reads operations at least to one instead of all the container
      // Basically this operation is costly when a huge amount of keys is stored
      for (const auto &_item : _index)
      {
#ifdef ENABLED_FEATURE_METRICS
        _item.metrics_->reads_.fetch_add(1, std::memory_order_relaxed);
#endif
        if (!_item.expired_)
        {
          if (_expires_at <= _item.entry_.expires_at_)
          {
            boost::asio::dispatch(
              state->strand_,
              boost::asio::bind_allocator(
                connection_handler_allocator<void>(state->create_scheduler_handler_memory_),
                [_state = state->shared_from_this(), _expires_at]
                { _state->garbage_collector_->schedule_timer(_state, _expires_at); }));
          }
          break;
        }
      }
    }

#ifndef NDEBUG
    if (_entry_ptr.entry_.type_ == entry_types::counter)
    {
      const value_type _counter_value = _entry_ptr.entry_.counter_.load(std::memory_order_relaxed);
      fmt::println(
        "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST INSERT session_id={} META key={} value={}ttl_type={} ttl={} RESPONSE ok={}",
        to_string(state->id_),
        std::chrono::system_clock::now(),
        to_string(id),
        span_to_hex(key),
        _counter_value,
        to_string(ttl_type),
        span_to_hex(ttl),
        _inserted);
    }
    else
    {
      auto _buffer = _entry_ptr.entry_.buffer_.load(std::memory_order_relaxed);
      fmt::println(
        "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST SET session_id={} META key={} value={}ttl_type={} ttl={} RESPONSE ok={}",
        to_string(state->id_),
        std::chrono::system_clock::now(),
        to_string(id),
        span_to_hex(key),
        span_to_hex(*_buffer),
        to_string(ttl_type),
        span_to_hex(ttl),
        _inserted);
    }
#endif

    return _inserted;
  }
} // namespace throttr
