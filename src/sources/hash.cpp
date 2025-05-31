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

#include <throttr/hash.hpp>

namespace throttr
{
  std::size_t hash::operator()(const std::array<std::byte, 16> &arr) const noexcept
  {
    std::size_t _hash = 0;
    for (const auto _byte : arr)
      _hash ^= std::hash<uint8_t>{}(std::to_integer<uint8_t>(_byte)) + 0x9e3779b9 + (_hash << 6) + (_hash >> 2);
    return _hash;
  }
} // namespace throttr