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

#include <throttr/services/garbage_collector_service.hpp>

#include <boost/uuid/uuid_io.hpp>
#include <throttr/state.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  void garbage_collector_service::schedule_timer(const std::shared_ptr<state> &state, const uint64_t proposed)
  {
#ifndef NDEBUG
    fmt::println(
      "[{}] [{:%Y-%m-%d %H:%M:%S}] GARBAGE COLLECTION SCHEDULED", to_string(state->id_), std::chrono::system_clock::now());
#endif

    const uint64_t _now = std::chrono::system_clock::now().time_since_epoch().count();
    if (proposed <= _now) // LCOV_EXCL_LINE Note: Partially tested.
    {
      run(state);
      return;
    }

    const auto _delay = proposed - _now;
    state->expiration_timer_.expires_after(std::chrono::nanoseconds(_delay));
    state->expiration_timer_.async_wait(
      [_self = shared_from_this(), _state = state->shared_from_this()](const boost::system::error_code &ec)
      {
        if (!ec)
          _self->run(_state);
      });
  }

  void garbage_collector_service::run(const std::shared_ptr<state> &state)
  {
    std::scoped_lock guard(state->mutex_);
#ifndef NDEBUG
    fmt::
      println("[{}] [{:%Y-%m-%d %H:%M:%S}] GARBAGE COLLECTION STARTED", to_string(state->id_), std::chrono::system_clock::now());
#endif

    const uint64_t _now = std::chrono::system_clock::now().time_since_epoch().count();
    auto &_index = state->storage_.get<tag_by_key>();

    std::vector<request_key> _to_expire;
    std::vector<request_key> _to_erase;
    std::uint64_t _next_expiration = std::numeric_limits<std::uint64_t>::max();
    for (const auto &_item : _index) // LCOV_EXCL_LINE Note: Partially tested.
    {
      const auto _expires_ns = _item.entry_.expires_at_.load(std::memory_order_acquire);

      if (_item.expired_) // LCOV_EXCL_LINE Note: Partially tested.
      {

        if (constexpr auto _grace_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(10)).count();
            _now - _expires_ns > _grace_ns) // LCOV_EXCL_LINE Note: Partially tested.
        {
          _to_erase.emplace_back(_item.key());
        }
        continue;
      }
      if (_item.entry_.expires_at_ <= _now) // LCOV_EXCL_LINE Note: Partially tested.
      {
        _to_expire.emplace_back(_item.key());
      }
      else
      {
        _next_expiration = std::min(_next_expiration, _expires_ns);
      }
    }

    for (const auto &_key : _to_expire) // LCOV_EXCL_LINE Note: Partially tested.
    {
      if (auto _it = _index.find(_key); _it != _index.end() && !_it->expired_) // LCOV_EXCL_LINE Note: Partially tested.
      {
        _index.modify(
          _it,
          [&state](entry_wrapper &entry)
          {
#ifndef NDEBUG
            fmt::println(
              "[{}] [{:%Y-%m-%d %H:%M:%S}] GARBAGE COLLECTOR MARKED KEY AS EXPIRED key={}",
              to_string(state->id_),
              std::chrono::system_clock::now(),
              std::string_view(
                reinterpret_cast<const char *>(entry.key_.data()), // NOSONAR
                entry.key_.size()));                               // NOSONAR
#endif
            entry.expired_ = true;
          });
      }
    }

    auto &_erase_index = state->storage_.get<tag_by_key>();
    for (const auto &_key : _to_erase) // LCOV_EXCL_LINE Note: Partially tested.
    {
      if (auto _it = _erase_index.find(_key);         // LCOV_EXCL_LINE Note: Partially tested.
          _it != _erase_index.end() && _it->expired_) // LCOV_EXCL_LINE Note: Partially tested.
      {
#ifndef NDEBUG
        fmt::println(
          "[{}] [{:%Y-%m-%d %H:%M:%S}] GARBAGE COLLECTOR ERASED EXPIRED KEY key={}",
          to_string(state->id_),
          std::chrono::system_clock::now(),
          std::string_view(reinterpret_cast<const char *>(_it->key_.data()), _it->key_.size())); // NOSONAR
#endif
        _erase_index.erase(_it);
      }
    }

    for (auto &_candidate_index = state->storage_.get<tag_by_key>(); // LCOV_EXCL_LINE Note: Partially tested.
         const auto &_item : _candidate_index)                       // LCOV_EXCL_LINE Note: Partially tested.
    {
      const auto _expires_at = _item.entry_.expires_at_.load(std::memory_order_acquire);
      uint64_t _candidate;

      if (_item.expired_) // LCOV_EXCL_LINE Note: Partially tested.
      {
        constexpr auto _grace_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(10)).count();
        _candidate = _expires_at + _grace_ns;
      }
      else
        _candidate = _expires_at;

      _next_expiration = std::min(_next_expiration, _candidate);
    }

#ifndef NDEBUG
    fmt::println(
      "[{}] [{:%Y-%m-%d %H:%M:%S}] GARBAGE COLLECTION COMPLETED", to_string(state->id_), std::chrono::system_clock::now());
#endif

    if (_next_expiration != std::numeric_limits<std::uint64_t>::max()) // LCOV_EXCL_LINE Note: Partially tested.
      schedule_timer(state, _next_expiration);
  }
} // namespace throttr
