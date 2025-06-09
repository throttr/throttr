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

#include <throttr/services/find_service.hpp>

#include <boost/core/ignore_unused.hpp>
#include <throttr/state.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  std::optional<storage_iterator> find_service::find_or_fail(const std::shared_ptr<state> &state, const request_key &key)
  {
    const auto &_index = state->storage_.get<tag_by_key>();
    auto _it = _index.find(key);
    if (_it == _index.end() || _it->expired_) // LCOV_EXCL_LINE
    {
      return std::nullopt;
    }

#ifdef ENABLED_FEATURE_METRICS
    _it->metrics_->reads_.fetch_add(1, std::memory_order_relaxed);
#endif

    return _it;
  }

  std::optional<storage_iterator> find_service::find_or_fail_for_batch(
    const std::shared_ptr<state> &state,
    const request_key &key,
    std::vector<boost::asio::const_buffer> &batch)
  {
    const auto _it = find_or_fail(state, key);
    if (!_it.has_value()) // LCOV_EXCL_LINE
    {
      batch.emplace_back(boost::asio::const_buffer(&state::failed_response_, 1));
    }
    return _it;
  }
} // namespace throttr
