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

#include <throttr/custom_allocator.hpp>

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

    buffers_pool::recycle();

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
            item.entry_.update_buffer(value);
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

    auto [_it, _inserted] = state->storage_.emplace(_key, type, value, ttl_type, _expires_at);

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
                custom_allocator<void>(state->create_scheduler_handler_memory_),
                [_state = state->shared_from_this(), _expires_at]
                { _state->garbage_collector_->schedule_timer(_state, _expires_at); }));
          }
          break;
        }
      }
    }

#ifndef NDEBUG
    if (as_insert)
    {
      fmt::println(
        "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST INSERT session_id={} META key={} ttl_type={} ttl={} RESPONSE ok={}",
        to_string(state->id_),
        std::chrono::system_clock::now(),
        to_string(id),
        span_to_hex(key),
        to_string(ttl_type),
        span_to_hex(ttl),
        _inserted);
    }
    else
    {
      fmt::println(
        "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST SET session_id={} META key={} ttl_type={} ttl={} RESPONSE ok={}",
        to_string(state->id_),
        std::chrono::system_clock::now(),
        to_string(id),
        span_to_hex(key),
        to_string(ttl_type),
        span_to_hex(ttl),
        _inserted);
    }
#endif

    return _inserted;
  }
} // namespace throttr
