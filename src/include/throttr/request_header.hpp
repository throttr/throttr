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

#ifndef THROTTR_REQUEST_HEADER_HPP
#define THROTTR_REQUEST_HEADER_HPP

#include <cstdint>

namespace throttr {
    /**
     * Request header
     */
#pragma pack(push, 1)
    struct request_header {
        uint8_t ip_version_;
        std::array<uint8_t, 16> ip_;
        uint16_t port_;
        uint8_t max_requests_;
        uint32_t ttl_;
        uint8_t size_;
    };
#pragma pack(pop)
}

#endif // THROTTR_REQUEST_HEADER_HPP
