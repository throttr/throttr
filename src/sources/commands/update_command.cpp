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

#include <iostream>
#include <throttr/commands/update_command.hpp>

#include <throttr/services/update_service.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <throttr/connection.hpp>
#include <throttr/services/find_service.hpp>
#include <throttr/state.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  void update_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer,
    const boost::uuids::uuid id)
  {

    boost::ignore_unused(type, write_buffer, id);

    const auto _request = request_update::from_buffer(view);

    std::string _key(_request.key_.size(), '\0');
    std::memcpy(_key.data(), _request.key_.data(), _request.key_.size());

    const request_key _request_key{_key};

    const std::uint64_t _now = std::chrono::system_clock::now().time_since_epoch().count();

    batch.reserve(batch.size() + 1);

    const auto _it = find_service::find_or_fail(state, _request_key);

    if (!_it.has_value())
    {
      batch.emplace_back(&state::failed_response_, 1);
#ifndef NDEBUG
      fmt::println(
        "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST UPDATE session_id={} META key={} RESPONSE ok=false",
        to_string(state->id_),
        std::chrono::system_clock::now(),
        to_string(id),
        span_to_hex(_request.key_));
#endif
      return;
    }

    using enum attribute_types;

    bool _modified = false;

    state->storage_.modify(
      _it.value(),
      [_request, state, &_modified, &_now](entry_wrapper &object)
      {
        switch (_request.attribute_)
        {
          case quota:
            if (object.entry_.type_ == entry_types::counter)
            {
              _modified = update_service::apply_quota_change(state, object.entry_, _request);
            }
            break;
          case ttl:
            _modified = update_service::apply_ttl_change(state, object.entry_, _request, _now, object.key_);
            break;
        }
      });

#ifdef ENABLED_FEATURE_METRICS
    if (_modified)
    {
      _it.value()->metrics_->writes_.fetch_add(1, std::memory_order_relaxed);
    }
#endif

#ifndef NDEBUG
    fmt::println(
      "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST UPDATE session_id={} META key={} attribute={} change={} value={} RESPONSE ok={}",
      to_string(state->id_),
      std::chrono::system_clock::now(),
      to_string(id),
      _request_key.key_,
      to_string(_request.attribute_),
      to_string(_request.change_),
      _request.value_,
      _modified);
#endif

    if (_modified)
    {
      batch.emplace_back(&state::success_response_, 1);
    }
    else
    {
      batch.emplace_back(&state::failed_response_, 1);
    }
  }
} // namespace throttr
