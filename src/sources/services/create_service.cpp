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
#include <boost/uuid/uuid_io.hpp>
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
    boost::ignore_unused(as_insert);

    const auto _now = std::chrono::steady_clock::now();
    const auto _expires_at = get_expiration_point(_now, ttl_type, ttl);

    auto &_index = state->storage_.get<tag_by_key>();
    const std::vector _key(
      key.data(),             // NOSONAR
      key.data() + key.size() // NOSONAR
    );
    const std::vector _value(
      value.data(),               // NOSONAR
      value.data() + value.size() // NOSONAR
    );
    const auto _entry_ptr = entry_wrapper{_key, request_entry{type, _value, ttl_type, _expires_at}};

#ifdef ENABLED_FEATURE_METRICS
    _entry_ptr.metrics_->writes_.fetch_add(1, std::memory_order_relaxed);
#endif

    auto [_it, _inserted] = state->storage_.insert(std::move(_entry_ptr));

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
        _item.metrics_->reads_.fetch_add(1, std::memory_order_relaxed);
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
    fmt::println(
      "{:%Y-%m-%d %H:%M:%S} REQUEST {} key={} from={} value={}ttl_type={} ttl={} "
      "RESPONSE ok={}",
      std::chrono::system_clock::now(),
      as_insert ? "INSERT" : "SET",
      span_to_hex(key),
      to_string(id),
      span_to_hex(_entry_ptr.entry_.value_),
      to_string(ttl_type),
      span_to_hex(ttl),
      _inserted);
#endif
    // LCOV_EXCL_STOP

    return _inserted;
  }
} // namespace throttr
