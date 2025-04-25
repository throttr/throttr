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

#ifndef THROTTR_REQUEST_HPP
#define THROTTR_REQUEST_HPP

#include <variant>
#include <span>
#include <string_view>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace throttr {
    struct request {
        /**
         * IP address
         */
        std::variant<std::monostate, std::span<const uint8_t, 4>, std::span<const uint8_t, 16>> ip;

        /**
         * Port
         */
        uint16_t port = 0;

        /**
         * URL
         */
        std::string_view url;

        /**
         * Max requests
         */
        uint8_t max_requests = 0;

        /**
         * TTL
         */
        uint32_t ttl_ms = 0;

        /**
         * From string
         *
         * @param buffer
         * @return request
         */
        static request from_string(std::string_view buffer);

        /**
         * To bytes
         *
         * @return vector<uint8_t>
         */
        [[nodiscard]] std::vector<uint8_t> to_bytes() const;
    };
}

#endif // THROTTR_REQUEST_HPP
