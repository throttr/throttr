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

#include <span>
#include <string_view>
#include <stdexcept>
#include <chrono>

namespace throttr {
    /**
     * Request header
     */
#pragma pack(push, 1)
    struct request_header {
        /**
         * IP version
         */
        uint8_t ip_version_;

        /**
         * IP address
         */
        std::array<uint8_t, 16> ip_;

        /**
         * Port
         */
        uint16_t port_;

        /**
         * Max requests
         */
        uint32_t max_requests_;

        /**
         * TTL
         */
        uint32_t ttl_;

        /**
         * URL size
         */
        uint8_t size_;
    };
#pragma pack(pop)
}


/**
 * Request key
 */
struct request_key {
    /**
     * IP version
     */
    uint8_t ip_version_;

    /**
     * IP address
     */
    std::array<uint8_t, 16> ip_;

    /**
     * Port
     */
    uint16_t port_;

    /**
     * URL
     */
    std::string url_;

    /**
     * Comparator
     *
     * @param other
     * @return bool
     */
    bool operator==(const request_key &other) const = default; // LCOV_EXCL_LINE note: Hash requirement.
};

/**
 * Request key hasher
 */
struct request_key_hasher {
    /**
     * Invoke
     *
     * @param key
     * @return size_t
     */
    std::size_t operator()(const request_key &key) const {
        std::size_t _h = std::hash<uint8_t>{}(key.ip_version_);

        for (const auto _b: key.ip_) {
            // LCOV_EXCL_LINE note: Partially tested.
            _h ^= std::hash<uint8_t>{}(_b) + 0x9e3779b9 + (_h << 6) + (_h >> 2);
        }

        _h ^= std::hash<uint16_t>{}(key.port_) + 0x9e3779b9 + (_h << 6) + (_h >> 2);
        _h ^= std::hash<std::string>{}(key.url_) + 0x9e3779b9 + (_h << 6) + (_h >> 2);

        return _h;
    }
};

/**
 * Request entry
 */
struct request_entry {
    /**
     * Available requests
     */
    int available_requests_ = 0;

    /**
     * Expires at
     */
    std::chrono::steady_clock::time_point expires_at_;
};

/**
 * Request error
 */
struct request_error final : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/**
 * Request view
 */
struct request_view {
    /**
     * Header
     */
    const request_header *header_ = nullptr;

    /**
     * URL
     */
    std::string_view url_;

    /**
     * From buffer
     *
     * @param buffer
     * @return request_view
     */
    static request_view from_buffer(const std::span<const std::byte> &buffer) {
        if (buffer.size() < sizeof(request_header)) {
            throw request_error("buffer too small for request");
        }

        const auto *_header = reinterpret_cast<const request_header *>(buffer.data());

        if (buffer.size() < sizeof(request_header) + _header->size_) {
            // LCOV_EXCL_LINE note: Partially tested.
            throw request_error("buffer too small for url payload");
        }

        const auto _url = buffer.subspan(sizeof(request_header), _header->size_);
        return request_view{
            _header,
            std::string_view(reinterpret_cast<const char *>(_url.data()), _url.size())
        };
    }

    /**
     * To buffer
     *
     * @return span<const uint8_t>
     */
    [[nodiscard]] std::span<const std::byte> to_buffer() const {
        return {
            reinterpret_cast<const std::byte *>(header_),
            sizeof(request_header) + url_.size()
        };
    }
};

}

#endif // THROTTR_REQUEST_HPP
