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
#include <throttr/utils.hpp>

namespace throttr
{
  std::uint8_t state::handle_update(const request_update &request)
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
            if (object.entry_.type_ == entry_types::counter) // LCOV_EXCL_LINE Note: Partially tested.
            {
              _modified = apply_quota_change(object.entry_, request);
            }
            break;
          case ttl:
            _modified = apply_ttl_change(object.entry_, request, _now, object.key_);
            break;
        }
      });

#ifdef ENABLED_FEATURE_METRICS
    if (_modified) // LCOV_EXCL_LINE Note: Partially tested.
    {
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

  bool state::apply_quota_change(request_entry &entry, const request_update &request)
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

  bool state::apply_ttl_change(
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
      boost::asio::
        post(strand_, [_self = shared_from_this(), _expires_at = entry.expires_at_] { _self->schedule_expiration(_expires_at); });
    }
    // LCOV_EXCL_STOP

    return true;
  }
} // namespace throttr