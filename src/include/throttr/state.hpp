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

#ifndef THROTTR_STATE_HPP
#define THROTTR_STATE_HPP

#include <throttr/request.hpp>

#include <memory>
#include <atomic>
#include <vector>
#include <chrono>
#include <unordered_map>

namespace throttr {
    struct request_key {
        uint8_t ip_version;
        std::array<uint8_t, 16> ip;
        uint16_t port;
        std::string url;

        bool operator==(const request_key& other) const {
            return ip_version == other.ip_version &&
           ip == other.ip &&
           port == other.port &&
           url == other.url;
        };
    };

    struct request_key_hasher {
        std::size_t operator()(const request_key& key) const {
            std::size_t h = std::hash<uint8_t>{}(key.ip_version);

            for (auto b : key.ip) {
                h ^= std::hash<uint8_t>{}(b) + 0x9e3779b9 + (h << 6) + (h >> 2);
            }

            h ^= std::hash<uint16_t>{}(key.port) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<std::string>{}(key.url) + 0x9e3779b9 + (h << 6) + (h >> 2);

            return h;
        }
    };

    struct request_entry {
        int available_requests = 0;
        std::chrono::steady_clock::time_point expires_at;
    };

    /**
     * State
     */
    class state : public std::enable_shared_from_this<state> {
    public:
        /**
         * Acceptor ready
         */
        std::atomic_bool acceptor_ready_;

        std::unordered_map<request_key, request_entry, request_key_hasher> requests_;

        /**
         * Handle request
         *
         * @param view
         * @return vector<byte>
         */
        std::vector<std::byte> handle_request(const request_view & view);
    };
}

#endif // THROTTR_STATE_HPP
