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

#include <throttr/services/update_service.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/endian/conversion.hpp>
#include <throttr/services/garbage_collector_service.hpp>
#include <throttr/state.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  bool update_service::apply_quota_change(const std::shared_ptr<state> &state, entry &entry, const request_update &request)
  {
    boost::ignore_unused(state);

    using enum change_types;
    using namespace boost::endian;

    switch (request.change_)
    {
      case patch:
        entry.counter_.store(little_to_native(request.value_), std::memory_order_relaxed);
        break;
      case increase:
        entry.counter_.fetch_add(little_to_native(request.value_), std::memory_order_relaxed);
        break;
      case decrease:
        if (entry.counter_.load(std::memory_order_relaxed) >= little_to_native(request.value_)) // LCOV_EXCL_LINE note: Partially
                                                                                                // covered.
        {
          entry.counter_.fetch_sub(little_to_native(request.value_), std::memory_order_relaxed);
        }
        else
        {
          return false;
        }
        break;
    }
    return true;
  }

  bool update_service::apply_ttl_change(
    const std::shared_ptr<state> &state,
    entry &entry,
    const request_update &request,
    const std::uint64_t now,
    std::span<const std::byte> key)
  {
    using namespace std::chrono;
    using namespace boost::endian;

    std::uint64_t _duration_ns = 0;

    switch (entry.ttl_type_)
    {
      case ttl_types::nanoseconds:
        _duration_ns = little_to_native(request.value_);
        break;
      case ttl_types::milliseconds:
        _duration_ns = std::chrono::duration_cast<nanoseconds>(milliseconds(little_to_native(request.value_))).count();
        break;
      case ttl_types::seconds:
        _duration_ns = std::chrono::duration_cast<nanoseconds>(seconds(little_to_native(request.value_))).count();
        break;
      case ttl_types::microseconds:
        _duration_ns = std::chrono::duration_cast<nanoseconds>(microseconds(little_to_native(request.value_))).count();
        break;
      case ttl_types::minutes:
        _duration_ns = std::chrono::duration_cast<nanoseconds>(minutes(little_to_native(request.value_))).count();
        break;
      case ttl_types::hours:
        _duration_ns = std::chrono::duration_cast<nanoseconds>(hours(little_to_native(request.value_))).count();
        break;
    }

    switch (request.change_)
    {
      case change_types::patch:
        entry.expires_at_.store(now + _duration_ns, std::memory_order_release);
        break;
      case change_types::increase:
        entry.expires_at_.fetch_add(_duration_ns, std::memory_order_acq_rel);
        break;
      case change_types::decrease:
        entry.expires_at_.fetch_sub(_duration_ns, std::memory_order_acq_rel);
        break;
    }

    // LCOV_EXCL_START
    if (
      state->scheduled_key_.size() == key.size() &&
      std::equal(state->scheduled_key_.begin(), state->scheduled_key_.end(), key.begin()))
    {
      boost::asio::post(
        state->strand_,
        [_state = state->shared_from_this(), _expires_at = entry.expires_at_.load(std::memory_order_relaxed)]
        { _state->garbage_collector_->schedule_timer(_state, _expires_at); });
    }
    // LCOV_EXCL_STOP

    return true;
  }
} // namespace throttr
