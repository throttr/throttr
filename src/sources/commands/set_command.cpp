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

#include <throttr/commands/set_command.hpp>

#include <throttr/services/create_service.hpp>
#include <throttr/state.hpp>

namespace throttr
{
  void set_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::uint8_t> &write_buffer)
  {
    boost::ignore_unused(type, batch, write_buffer);

    const auto [header_, key_, value_] = request_set::from_buffer(view);
    const std::vector _value(value_.begin(), value_.end());

    const auto _inserted = create_service::use(state, key_, _value, header_->ttl_type_, header_->ttl_, entry_types::raw);
    batch.emplace_back(boost::asio::buffer(_inserted ? &state::success_response_ : &state::failed_response_, 1));
  }
} // namespace throttr
