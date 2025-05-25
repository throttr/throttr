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
  std::uint8_t state::handle_insert(const std::span<const std::byte> view)
  {
    const auto [header_, key_] = request_insert::from_buffer(view);
    std::vector<std::byte> value(sizeof(value_type));
    std::memcpy(value.data(), &header_->quota_, sizeof(value_type));
    return handle_create(key_, value, header_->ttl_type_, header_->ttl_, entry_types::counter);
  }
} // namespace throttr