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

#ifndef THROTTR_PROTOCOL_WRAPPER_HPP
#define THROTTR_PROTOCOL_WRAPPER_HPP

/**
 * This rewrites the size of Quota and TTL.
 */
#if defined(THROTTR_VALUE_SIZE_UINT8)
#define THROTTR_VALUE_SIZE uint8_t
#elif defined(THROTTR_VALUE_SIZE_UINT16)
#define THROTTR_VALUE_SIZE uint16_t
#elif defined(THROTTR_VALUE_SIZE_UINT32)
#define THROTTR_VALUE_SIZE uint32_t
#elif defined(THROTTR_VALUE_SIZE_UINT64)
#define THROTTR_VALUE_SIZE uint64_t
#endif

#include <throttr/protocol.hpp>

#endif // THROTTR_PROTOCOL_WRAPPER_HPP
