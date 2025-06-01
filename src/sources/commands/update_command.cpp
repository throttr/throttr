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

#include <throttr/commands/update_command.hpp>

#include <throttr/services/update_service.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <throttr/connection.hpp>
#include <throttr/state.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  void update_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::uint8_t> &write_buffer,
    const std::shared_ptr<connection> &conn)
  {

    boost::ignore_unused(type, write_buffer, conn);

    const auto _request = request_update::from_buffer(view);
    const request_key _key{std::string_view(reinterpret_cast<const char *>(_request.key_.data()), _request.key_.size())};
    const auto _now = std::chrono::steady_clock::now();
    const auto _it = state->finder_->find_or_fail(state, _key);

    if (!_it.has_value()) // LCOV_EXCL_LINE note: Partially covered.
    {
      batch.emplace_back(boost::asio::buffer(&state::failed_response_, 1));
      return;
    }

    using enum attribute_types;

    bool _modified = false;

    state->storage_.modify(
      _it.value(),
      [&](entry_wrapper &object)
      {
        switch (_request.attribute_)
        {
          case quota:
            if (object.entry_.type_ == entry_types::counter) // LCOV_EXCL_LINE Note: Partially tested.
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
    if (_modified) // LCOV_EXCL_LINE Note: Partially tested.
    {
      _it.value()->metrics_->writes_.fetch_add(1, std::memory_order_relaxed);
    }
#endif

    // LCOV_EXCL_START
#ifndef NDEBUG
    fmt::println(
      "{:%Y-%m-%d %H:%M:%S} REQUEST UPDATE key={} attribute={} change={} from={} "
      "value={} RESPONSE ok={}",
      std::chrono::system_clock::now(),
      _key.key_,
      to_string(_request.attribute_),
      to_string(_request.change_),
      to_string(conn->id_),
      _request.value_,
      _modified);
#endif
    // LCOV_EXCL_STOP

    if (_modified)
    {
      batch.emplace_back(boost::asio::buffer(&state::success_response_, 1));
    }
    else
    {
      batch.emplace_back(boost::asio::buffer(&state::failed_response_, 1));
    }
  }
} // namespace throttr
