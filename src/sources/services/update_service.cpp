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
#include <throttr/state.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
    bool update_service::apply_quota_change(const std::shared_ptr<state> &state, request_entry &entry,
    const request_update &request) {
        boost::ignore_unused(state);

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

    bool update_service::apply_ttl_change(const std::shared_ptr<state> &state, request_entry &entry,
        const request_update &request, const std::chrono::steady_clock::time_point &now,
        std::span<const std::byte> key) {
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
        if (state->scheduled_key_.size() == key.size() && std::equal(state->scheduled_key_.begin(), state->scheduled_key_.end(), key.begin()))
        {
            boost::asio::post(
              state->strand_,
              [_state = state->shared_from_this(), _expires_at = entry.expires_at_] { _state->garbage_collector_->schedule_timer(_state, _expires_at); });
        }
        // LCOV_EXCL_STOP

        return true;
    }
} // namespace throttr
