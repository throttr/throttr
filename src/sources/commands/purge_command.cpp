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

#include <throttr/commands/purge_command.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <throttr/connection.hpp>
#include <throttr/state.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  void purge_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer,
    const boost::uuids::uuid id)
  {

    boost::ignore_unused(type, write_buffer, id);

    const auto _request = request_purge::from_buffer(view);
    const request_key _key{
      std::string_view(reinterpret_cast<const char *>(_request.key_.data()), _request.key_.size())}; // NOSONAR

    auto &_index = state->storage_.get<tag_by_key>();
    const auto _it = _index.find(_key);

    bool _erased = true;

    if (_it == _index.end() || _it->expired_) // LCOV_EXCL_LINE note: Partially covered.
    {
      _erased = false;
    }

#ifdef ENABLED_FEATURE_METRICS
    if (_it != _index.end()) // LCOV_EXCL_LINE Note: Partially tested.
    {
      _it->metrics_->reads_.fetch_add(1, std::memory_order_relaxed);
    }
#endif

    // LCOV_EXCL_START
#ifndef NDEBUG
    fmt::println(
      "{:%Y-%m-%d %H:%M:%S} REQUEST PURGE session_id={} META key={} RESPONSE ok={}",
      std::chrono::system_clock::now(),
      to_string(id),
      _key.key_,
      _erased);
#endif
    // LCOV_EXCL_STOP

    if (_erased) // LCOV_EXCL_LINE Note: Partially tested.
      _index.erase(_it);

    if (_erased)
    {
      batch.emplace_back(boost::asio::buffer(&state::success_response_, 1));
    }
    else
    {
      batch.emplace_back(boost::asio::buffer(&state::failed_response_, 1));
    }
  }
} // namespace throttr
