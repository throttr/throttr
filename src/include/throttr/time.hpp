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

#ifndef THROTTR_TIME_HPP
#define THROTTR_TIME_HPP

#include <throttr/protocol.hpp>

#include <chrono>

namespace throttr {
    /**
     * Get expiration point
     *
     * @param now
     * @param ttl_type
     * @param ttl
     * @return std::chrono::steady_clock::time_point
     */
    inline std::chrono::steady_clock::time_point get_expiration_point(
        const std::chrono::steady_clock::time_point& now,
        const ttl_types ttl_type,
        const uint16_t ttl
    ) {
        using namespace std::chrono;

        switch (ttl_type) {
            case ttl_types::nanoseconds:
                return now + nanoseconds(ttl);
            case ttl_types::milliseconds:
                return now + milliseconds(ttl);
            default:
                return now + seconds(ttl);
        }
    }

    /**
     * Get TTL
     *
     * @param expires_at
     * @param ttl_type
     * @return uint16_t
     */
    inline uint16_t get_ttl(
        const std::chrono::steady_clock::time_point& expires_at,
        const ttl_types ttl_type
    ) {
        const auto now = std::chrono::steady_clock::now();
        if (expires_at <= now) {
            return 0;
        }

        const auto diff = expires_at - now;

        switch (ttl_type) {
            case ttl_types::nanoseconds:
                return std::chrono::duration_cast<std::chrono::nanoseconds>(diff).count();
            case ttl_types::milliseconds:
                return std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
            default:
                return std::chrono::duration_cast<std::chrono::seconds>(diff).count();
        }
    }
}


#endif // THROTTR_TIME_HPP