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
     * Request types
     */
    enum class request_types : uint8_t {
        /**
         * Insert
         */
        insert = 0x01,

        /**
         * Query
         */
        query = 0x02,

        /**
         * Update
         */
        update = 0x03,

        /**
         * Purge
         */
        purge = 0x04,
    };

    /**
     * TTL types
     */
    enum class ttl_types : uint8_t {
        /**
         * Nanoseconds
         */
        nanoseconds = 0x00,

        /**
         * Milliseconds
         */
        milliseconds = 0x01,

        /**
         * Seconds
         */
        seconds = 0x02,
    };

    /**
     * Attribute type
     */
    enum class attribute_types : uint8_t {
        /**
         * Quota
         */
        quota = 0x00,

        /**
         * TTL
         */
        ttl = 0x01,
    };

    /**
     * Change types
     */
    enum class change_types : uint8_t {
        /**
         * Patch
         */
        patch = 0x00,

        /**
         * Increase
         */
        increase = 0x01,

        /**
         * Decrease
         */
        decrease = 0x02,
    };


#pragma pack(push, 1)
    /**
     * Request insert header size
     */
    constexpr std::size_t request_insert_header_size = 28;

    /**
     * Request insert header
     */
    struct request_insert_header {
        /**
         * Request type
         */
        request_types request_type_;

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
        ttl_types ttl_type_;

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
    /**
     * Request query header size
     */
    constexpr std::size_t request_query_header_size = 3;

    /**
     * Request query header
     */
    struct request_query_header {
        /**
         * Request type
         */
        request_types request_type_;

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
    /**
     * Request update header size
     */
    constexpr std::size_t request_update_header_size = 13;

    /**
     * Request update header
     */
    struct request_update_header {
        /**
         * Request type
         */
        request_types request_type_;

        /**
         * Attribute
         */
        attribute_types attribute_;

        /**
         * Change
         */
        change_types change_;

        /**
         * Value
         */
        uint64_t value_;

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
    /**
     * Request purge header size
     */
    constexpr std::size_t request_purge_header_size = 3;

    /**
     * Request purge header
     */
    struct request_purge_header {
        /**
         * Request type
         */
        request_types request_type_;

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
     * Request update
     */
    struct request_update {
        /**
         * Header
         */
        const request_update_header *header_ = nullptr;

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
         * @return request_update
         */
        static request_update from_buffer(const std::span<const std::byte> &buffer) {
            if (buffer.size() < request_update_header_size) {
                throw request_error("buffer too small for request_update");
            }

            const auto *_header = reinterpret_cast<const request_update_header *>(buffer.data()); // NOSONAR

            if (const auto _expected = static_cast<std::size_t>(_header->consumer_id_size_) + _header->resource_id_size_
                ; buffer.size() < request_update_header_size + _expected) {
                throw request_error("buffer too small for request_update payload");
            }

            const auto _consumer_id = buffer.subspan(request_update_header_size, _header->consumer_id_size_);
            const auto _resource_id = buffer.subspan(request_update_header_size + _header->consumer_id_size_,
                                                     _header->resource_id_size_);

            return request_update{
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
            _buffer.resize(request_update_header_size + consumer_id_.size() + resource_id_.size());

            std::memcpy(_buffer.data(), header_, request_update_header_size);
            std::memcpy(_buffer.data() + request_update_header_size, consumer_id_.data(), consumer_id_.size());
            std::memcpy(_buffer.data() + request_update_header_size + consumer_id_.size(), resource_id_.data(),
                        resource_id_.size());

            return _buffer;
        }
    };


    /**
     * Request purge
     */
    struct request_purge {
        /**
         * Header
         */
        const request_purge_header *header_ = nullptr;

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
         * @return request_purge
         */
        static request_purge from_buffer(const std::span<const std::byte> &buffer) {
            if (buffer.size() < request_purge_header_size) {
                throw request_error("buffer too small for request_purge");
            }

            const auto *_header = reinterpret_cast<const request_purge_header *>(buffer.data()); // NOSONAR

            if (const auto _expected = _header->consumer_id_size_ + _header->resource_id_size_
                ; buffer.size() < request_purge_header_size + _expected) {
                throw request_error("buffer too small for request_purge payload");
            }

            const auto _consumer_id = buffer.subspan(request_purge_header_size, _header->consumer_id_size_);
            const auto _resource_id = buffer.subspan(request_purge_header_size + _header->consumer_id_size_,
                                                     _header->resource_id_size_);

            return request_purge{
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
            _buffer.resize(request_purge_header_size + consumer_id_.size() + resource_id_.size());

            std::memcpy(_buffer.data(), header_, request_purge_header_size);
            std::memcpy(_buffer.data() + request_purge_header_size, consumer_id_.data(), consumer_id_.size());
            std::memcpy(_buffer.data() + request_purge_header_size + consumer_id_.size(), resource_id_.data(),
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
        ttl_types ttl_type_ = ttl_types::nanoseconds;

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
        const ttl_types ttl_type = ttl_types::milliseconds,
        const uint64_t ttl = 0,
        const std::string_view consumer_id = "",
        const std::string_view resource_id = ""
    ) {
        std::vector<std::byte> _buffer;
        _buffer.resize(request_insert_header_size + consumer_id.size() + resource_id.size());

        auto *_header = reinterpret_cast<request_insert_header *>(_buffer.data()); // NOSONAR
        _header->request_type_ = request_types::insert;
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
        _header->request_type_ = request_types::query;
        _header->consumer_id_size_ = static_cast<uint8_t>(consumer_id.size());
        _header->resource_id_size_ = static_cast<uint8_t>(resource_id.size());

        std::memcpy(_buffer.data() + request_query_header_size, consumer_id.data(), consumer_id.size());
        std::memcpy(_buffer.data() + request_query_header_size + consumer_id.size(), resource_id.data(),
                    resource_id.size());

        return _buffer;
    }


    /**
     * Request purge builder
     *
     * @param consumer_id
     * @param resource_id
     * @return std::vector<std::byte>
     */
    inline std::vector<std::byte> request_purge_builder(
        const std::string_view consumer_id = "",
        const std::string_view resource_id = ""
    ) {
        std::vector<std::byte> _buffer;
        _buffer.resize(request_purge_header_size + consumer_id.size() + resource_id.size());

        auto *_header = reinterpret_cast<request_purge_header *>(_buffer.data()); // NOSONAR
        _header->request_type_ = request_types::purge;
        _header->consumer_id_size_ = static_cast<uint8_t>(consumer_id.size());
        _header->resource_id_size_ = static_cast<uint8_t>(resource_id.size());

        std::memcpy(_buffer.data() + request_purge_header_size, consumer_id.data(), consumer_id.size());
        std::memcpy(_buffer.data() + request_purge_header_size + consumer_id.size(), resource_id.data(),
                    resource_id.size());

        return _buffer;
    }

    /**
     * Request update builder
     *
     * @param attribute
     * @param change
     * @param value
     * @param consumer_id
     * @param resource_id
     * @return std::vector<std::byte>
     */
    inline std::vector<std::byte> request_update_builder(
        const attribute_types attribute = attribute_types::quota,
        const change_types change = change_types::patch,
        const uint64_t value = 0,
        const std::string_view consumer_id = "",
        const std::string_view resource_id = ""
    ) {
        std::vector<std::byte> _buffer;
        _buffer.resize(request_update_header_size + consumer_id.size() + resource_id.size());

        auto *_header = reinterpret_cast<request_update_header *>(_buffer.data()); // NOSONAR
        _header->request_type_ = request_types::update;
        _header->attribute_ = attribute;
        _header->change_ = change;
        _header->value_ = value;
        _header->consumer_id_size_ = static_cast<uint8_t>(consumer_id.size());
        _header->resource_id_size_ = static_cast<uint8_t>(resource_id.size());

        std::memcpy(_buffer.data() + request_update_header_size, consumer_id.data(), consumer_id.size());
        std::memcpy(_buffer.data() + request_update_header_size + consumer_id.size(), resource_id.data(),
                    resource_id.size());

        return _buffer;
    }
}

#endif // THROTTR_REQUEST_HPP
