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

#pragma once

#ifndef THROTTR_FRAGMENTS_LIST_HPP
#define THROTTR_FRAGMENTS_LIST_HPP
#include <cstdint>
#include <throttr/protocol_wrapper.hpp>

namespace throttr
{
#pragma pack(push, 1)
  struct fragment_list_item
  {
    std::uint8_t key_size_;
    entry_types key_type_;
    ttl_types ttl_type_;
    std::chrono::steady_clock::time_point expires_at_;
    value_type bytes_used_;
  };
#pragma pack(pop)
} // namespace throttr

#endif // THROTTR_FRAGMENTS_LIST_HPP
