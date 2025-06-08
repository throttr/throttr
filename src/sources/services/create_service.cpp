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
  bool create_service::use( // NOSONAR
    const std::shared_ptr<state> &state,
    std::span<const std::byte> key,
    std::span<const std::byte> value,
    const ttl_types ttl_type,
    std::span<const std::byte> ttl,
    const entry_types type,
    const boost::uuids::uuid &id,
    const bool as_insert) // NOSONAR
  {
    boost::ignore_unused(as_insert, id);

    using namespace boost::endian;

    const auto _now = std::chrono::steady_clock::now();
    const auto _now_ns = _now.time_since_epoch().count();
    const auto _expires_at = get_expiration_point(_now_ns, ttl_type, ttl);

    auto &_index = state->storage_.get<tag_by_key>();
    const std::vector _key(
      key.data(),             // NOSONAR
      key.data() + key.size() // NOSONAR
    );

    if (!as_insert)
    {
      const request_key _lookup_key{std::string_view(reinterpret_cast<const char *>(_key.data()), _key.size())}; // NOSONAR
      auto _it_existing = _index.find(_lookup_key);
      if (_it_existing != _index.end()) // LCOV_EXCL_LINE Note: Partially tested.
      {
        if (_it_existing->entry_.type_ != entry_types::counter) // LCOV_EXCL_LINE Note: Partially tested.
        {
          auto _modified = _index.modify(
            _it_existing,
            [&](entry_wrapper &item)
            {
              const auto _safe_buffer = std::make_shared<std::vector<std::byte>>(value.begin(), value.end());
              item.entry_.buffer_.store(_safe_buffer, std::memory_order_release);
              item.entry_.expires_at_ = _expires_at;
              item.entry_.ttl_type_ = ttl_type;

#ifdef ENABLED_FEATURE_METRICS
              item.metrics_->writes_.fetch_add(1, std::memory_order_relaxed);
#endif
            });

#ifndef NDEBUG
          fmt::println(
            "{:%Y-%m-%d %H:%M:%S} REQUEST SET AGAIN session_id={} META key={} value={}ttl_type={} ttl={} RESPONSE ok=true"
            "RESPONSE ok={}",
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
      _entry.buffer_
        .store(std::make_shared<std::vector<std::byte>>(value.data(), value.data() + value.size()), std::memory_order_release);
    }

    const auto _entry_ptr = entry_wrapper{_key, _entry};

#ifdef ENABLED_FEATURE_METRICS
    _entry_ptr.metrics_->writes_.fetch_add(1, std::memory_order_relaxed);
#endif

    auto [_it, _inserted] = state->storage_.insert(_entry_ptr);

    boost::ignore_unused(_it);

    // LCOV_EXCL_START Note: Actually tested
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
            boost::asio::post(
              state->strand_,
              [_state = state->shared_from_this(), _expires_at]
              { _state->garbage_collector_->schedule_timer(_state, _expires_at); });
          }
          break;
        }
      }
    }
    // LCOV_EXCL_STOP

    // LCOV_EXCL_START
#ifndef NDEBUG
    if (_entry_ptr.entry_.type_ == entry_types::counter)
    {
      const value_type _counter_value = _entry_ptr.entry_.counter_.load(std::memory_order_relaxed);
      fmt::println(
        "{:%Y-%m-%d %H:%M:%S} REQUEST INSERT session_id={} META key={} value={}ttl_type={} ttl={} RESPONSE ok={}",
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
        "{:%Y-%m-%d %H:%M:%S} REQUEST SET session_id={} META key={} value={}ttl_type={} ttl={} RESPONSE ok={}",
        std::chrono::system_clock::now(),
        to_string(id),
        span_to_hex(key),
        span_to_hex(*_buffer),
        to_string(ttl_type),
        span_to_hex(ttl),
        _inserted);
    }
#endif
    // LCOV_EXCL_STOP

    return _inserted;
  }
} // namespace throttr
