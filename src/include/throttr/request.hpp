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
#include <string>
#include <cstring>

namespace throttr {

    /**
     * Request error
     */
    struct request_error final : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    /**
     * Request type
     */
    enum class request_type : uint8_t {
        /**
         * Insert
         */
        insert = 0x01,
        /**
         * Query
         */
        query = 0x02
    };


#pragma pack(push, 1)
    constexpr std::size_t request_insert_header_size = 28;

    /**
     * Request insert header
     */
    struct request_insert_header {
        /**
         * Request type
         */
        request_type request_type_;

        /**
         * Quota
         */
        uint64_t quota_;

        /**
         * Usage
         */
        uint64_t usage_;

        /**
         * TTL type
         */
        uint8_t ttl_type_;

        /**
         * TTL
         */
        uint64_t ttl_;

        /**
         * Consumer ID size
         */
        uint8_t consumer_id_size_;

        /**
         * Resource ID size
         */
        uint8_t resource_id_size_;
    };
#pragma pack(pop)

#pragma pack(push, 1)
    constexpr std::size_t request_query_header_size = 3;

    /**
     * Request query header
     */
    struct request_query_header {
        /**
         * Request type
         */
        request_type request_type_;

        /**
         * Consumer ID size
         */
        uint8_t consumer_id_size_;

        /**
         * Resource ID size
         */
        uint8_t resource_id_size_;
    };
#pragma pack(pop)

    /**
     * Request insert
     */
    struct request_insert {
        /**
         * Header
         */
        const request_insert_header *header_ = nullptr;

        /**
         * Consumer ID
         */
        std::string_view consumer_id_;

        /**
         * Resource ID
         */
        std::string_view resource_id_;

        /**
         * From buffer
         *
         * @param buffer
         * @return request_insert
         */
        static request_insert from_buffer(const std::span<const std::byte> &buffer) {
            if (buffer.size() < request_insert_header_size) {
                throw request_error("buffer too small for request_insert");
            }

            const auto *_header = reinterpret_cast<const request_insert_header *>(buffer.data()); // NOSONAR

            if (const auto _expected = static_cast<std::size_t>(_header->consumer_id_size_) + _header->resource_id_size_
                ; buffer.size() < request_insert_header_size + _expected) {
                throw request_error("buffer too small for request_insert payload");
            }

            const auto _consumer_id = buffer.subspan(request_insert_header_size, _header->consumer_id_size_);
            const auto _resource_id = buffer.subspan(request_insert_header_size + _header->consumer_id_size_,
                                                     _header->resource_id_size_);

            return request_insert{
                _header,
                std::string_view(reinterpret_cast<const char *>(_consumer_id.data()), _consumer_id.size()), // NOSONAR
                std::string_view(reinterpret_cast<const char *>(_resource_id.data()), _resource_id.size()) // NOSONAR
            };
        }

        /**
         * To buffer
         *
         * @return std::vector<std::byte>
         */
        [[nodiscard]]
        std::vector<std::byte> to_buffer() const {
            std::vector<std::byte> _buffer;
            _buffer.resize(request_insert_header_size + consumer_id_.size() + resource_id_.size());

            std::memcpy(_buffer.data(), header_, request_insert_header_size);
            std::memcpy(_buffer.data() + request_insert_header_size, consumer_id_.data(), consumer_id_.size());
            std::memcpy(_buffer.data() + request_insert_header_size + consumer_id_.size(), resource_id_.data(),
                        resource_id_.size());

            return _buffer;
        }
    };

    /**
     * Request query
     */
    struct request_query {
        /**
         * Header
         */
        const request_query_header *header_ = nullptr;

        /**
         * Consumer ID
         */
        std::string_view consumer_id_;

        /**
         * Resource ID
         */
        std::string_view resource_id_;

        /**
         * From buffer
         *
         * @param buffer
         * @return request_query
         */
        static request_query from_buffer(const std::span<const std::byte> &buffer) {
            if (buffer.size() < request_query_header_size) {
                throw request_error("buffer too small for request_query");
            }

            const auto *_header = reinterpret_cast<const request_query_header *>(buffer.data()); // NOSONAR

            if (const auto _expected = _header->consumer_id_size_ + _header->resource_id_size_
                ; buffer.size() < request_query_header_size + _expected) {
                throw request_error("buffer too small for request_query payload");
            }

            const auto _consumer_id = buffer.subspan(request_query_header_size, _header->consumer_id_size_);
            const auto _resource_id = buffer.subspan(request_query_header_size + _header->consumer_id_size_,
                                                     _header->resource_id_size_);

            return request_query{
                _header,
                std::string_view(reinterpret_cast<const char *>(_consumer_id.data()), _consumer_id.size()), // NOSONAR
                std::string_view(reinterpret_cast<const char *>(_resource_id.data()), _resource_id.size()) // NOSONAR
            };
        }

        /**
         * To buffer
         *
         * @return std::vector<std::byte>
         */
        [[nodiscard]]
        std::vector<std::byte> to_buffer() const {
            std::vector<std::byte> _buffer;
            _buffer.resize(request_query_header_size + consumer_id_.size() + resource_id_.size());

            std::memcpy(_buffer.data(), header_, request_query_header_size);
            std::memcpy(_buffer.data() + request_query_header_size, consumer_id_.data(), consumer_id_.size());
            std::memcpy(_buffer.data() + request_query_header_size + consumer_id_.size(), resource_id_.data(),
                        resource_id_.size());

            return _buffer;
        }
    };

    /**
     * Request key
     */
    struct request_key {
        /**
         * Consumer ID
         */
        std::string consumer_id_;

        /**
         * Resource ID
         */
        std::string resource_id_;

        /**
         * Comparator
         *
         * @param other
         * @return bool
         */
        bool operator==(const request_key &other) const = default;
    };

    /**
     * Request key hasher
     */
    struct request_key_hasher {
        /**
         * Invoke
         *
         * @param key
         * @return std::size_t
         */
        std::size_t operator()(const request_key &key) const {
            std::size_t _h = std::hash<std::string>{}(key.consumer_id_);
            _h ^= std::hash<std::string>{}(key.resource_id_) + 0x9e3779b9 + (_h << 6) + (_h >> 2);
            return _h;
        }
    };

    /**
     * Request entry
     */
    struct request_entry {
        /**
         * Quota
         */
        uint64_t quota_ = 0;

        /**
         * TTL type
         */
        uint8_t ttl_type_ = 0;

        /**
         * Expires at
         */
        std::chrono::steady_clock::time_point expires_at_;
    };

    /**
     * Request insert builder
     *
     * @param quota
     * @param usage
     * @param ttl_type
     * @param ttl
     * @param consumer_id
     * @param resource_id
     * @return std::vector<std::byte>
     */
    inline std::vector<std::byte> request_insert_builder(
        const uint64_t quota = 0,
        const uint64_t usage = 0,
        const uint8_t ttl_type = 0,
        const uint64_t ttl = 0,
        const std::string_view consumer_id = "",
        const std::string_view resource_id = ""
    ) {
        std::vector<std::byte> _buffer;
        _buffer.resize(request_insert_header_size + consumer_id.size() + resource_id.size());

        auto *_header = reinterpret_cast<request_insert_header *>(_buffer.data()); // NOSONAR
        _header->request_type_ = request_type::insert;
        _header->quota_ = quota;
        _header->usage_ = usage;
        _header->ttl_type_ = ttl_type;
        _header->ttl_ = ttl;
        _header->consumer_id_size_ = static_cast<uint8_t>(consumer_id.size());
        _header->resource_id_size_ = static_cast<uint8_t>(resource_id.size());

        std::memcpy(_buffer.data() + request_insert_header_size, consumer_id.data(), consumer_id.size());
        std::memcpy(_buffer.data() + request_insert_header_size + consumer_id.size(), resource_id.data(),
                    resource_id.size());

        return _buffer;
    }

    /**
     * Request query builder
     *
     * @param consumer_id
     * @param resource_id
     * @return std::vector<std::byte>
     */
    inline std::vector<std::byte> request_query_builder(
        const std::string_view consumer_id = "",
        const std::string_view resource_id = ""
    ) {
        std::vector<std::byte> _buffer;
        _buffer.resize(request_query_header_size + consumer_id.size() + resource_id.size());

        auto *_header = reinterpret_cast<request_query_header *>(_buffer.data()); // NOSONAR
        _header->request_type_ = request_type::query;
        _header->consumer_id_size_ = static_cast<uint8_t>(consumer_id.size());
        _header->resource_id_size_ = static_cast<uint8_t>(resource_id.size());

        std::memcpy(_buffer.data() + request_query_header_size, consumer_id.data(), consumer_id.size());
        std::memcpy(_buffer.data() + request_query_header_size + consumer_id.size(), resource_id.data(),
                    resource_id.size());

        return _buffer;
    }
}

#endif // THROTTR_REQUEST_HPP
