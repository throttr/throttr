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

#include <throttr/state.hpp>

#include <fmt/chrono.h>

namespace throttr
{
  void state::expiration_timer()
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
    for (const auto &_item : _index) // LCOV_EXCL_LINE Note: Partially tested.
    {
      if (_item.expired_) // LCOV_EXCL_LINE Note: Partially tested.
      {
        if (_now - _item.entry_.expires_at_ > std::chrono::seconds(10)) // LCOV_EXCL_LINE Note: Partially tested.
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
        _next_expiration = std::min(_next_expiration, _item.entry_.expires_at_);
      }
    }

    for (const auto &_key : _to_expire) // LCOV_EXCL_LINE Note: Partially tested.
    {
      if (auto _it = _index.find(_key); _it != _index.end() && !_it->expired_) // LCOV_EXCL_LINE Note: Partially tested.
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
    for (const auto &_key : _to_erase) // LCOV_EXCL_LINE Note: Partially tested.
    {
      if (auto _it = _erase_index.find(_key);
          _it != _erase_index.end() && _it->expired_) // LCOV_EXCL_LINE Note: Partially tested.
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

    for (auto &_candidate_index = storage_.get<tag_by_key>();
         const auto &_item : _candidate_index) // LCOV_EXCL_LINE Note: Partially tested.
    {
      const auto &_expires_at = _item.entry_.expires_at_;
      std::chrono::steady_clock::time_point _candidate;

      if (_item.expired_) // LCOV_EXCL_LINE Note: Partially tested.
        _candidate = _expires_at + std::chrono::seconds(10);
      else
        _candidate = _expires_at;

      _next_expiration = std::min(_next_expiration, _candidate);
    }

#ifndef NDEBUG
    fmt::println("{:%Y-%m-%d %H:%M:%S} GARBAGE COLLECTION COMPLETED", std::chrono::system_clock::now());
#endif

    if (_next_expiration != std::chrono::steady_clock::time_point::max()) // LCOV_EXCL_LINE Note: Partially tested.
      schedule_expiration(_next_expiration);
  }

  void state::schedule_expiration(const std::chrono::steady_clock::time_point proposed)
  {
#ifndef NDEBUG
    fmt::println("{:%Y-%m-%d %H:%M:%S} GARBAGE COLLECTION SCHEDULED", std::chrono::system_clock::now());
#endif

    const auto _now = std::chrono::steady_clock::now();
    if (proposed <= _now)
    {
      expiration_timer();
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
} // namespace throttr