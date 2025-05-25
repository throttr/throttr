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

namespace throttr
{
  std::uint8_t state::handle_set(const std::span<const std::byte> view)
  {
    const auto [header_, key_, value_] = request_set::from_buffer(view);
    const std::vector value(value_.begin(), value_.end());
    return handle_create(key_, value, header_->ttl_type_, header_->ttl_, entry_types::raw);
  }
} // namespace throttr