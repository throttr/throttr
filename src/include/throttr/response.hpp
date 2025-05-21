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

#ifndef THROTTR_RESPONSE_HPP
#define THROTTR_RESPONSE_HPP

#include <throttr/protocol_wrapper.hpp>
#include <throttr/entry.hpp>

#include <boost/asio/buffer.hpp>

namespace throttr {
    /**
     * Response holder
     */
    struct response_holder {
        /**
         * Buffers
         */
        std::array<boost::asio::const_buffer, 5> buffers_;

        /**
         * Buffers size
         */
        std::size_t buffers_size_ = 0;

        /**
         * Status
         */
        std::uint8_t status_ = 0;

        /**
         * TTL type
         */
        std::uint8_t ttl_type_ = 0;

        /**
         * TTL
         */
        value_type ttl_ = 0;

        /**
         * Value size
         */
        value_type value_size_ = 0;

        /**
         * Disable copy
         */
        response_holder(const response_holder&) = delete;

        /**
         * Disable copy
         * @return
         */
        response_holder& operator=(const response_holder&) = delete;

        /**
         * Disable assignment copy
         */
        response_holder(response_holder&&) noexcept = default;

        /**
         * Disable assignment copy
         * @return
         */
        response_holder& operator=(response_holder&&) noexcept = default;

        /**
         * Constructor
         */
        response_holder() noexcept = default;

        void* operator new(std::size_t) = delete;
        void* operator new[](std::size_t) = delete;
        void* operator new(std::size_t, void* ptr) noexcept { return ptr; }

        /**
         * Constructor
         *
         * @param entry
         * @param ttl
         * @param as_query
         */
        response_holder(const entry &entry, const value_type ttl, const bool as_query = false)
            : status_(0x01),
              ttl_type_(static_cast<uint8_t>(entry.ttl_type_)),
              ttl_(ttl),
              value_size_(static_cast<value_type>(entry.value_.view().size_)) {
            const auto _view = entry.value_.view();
            if (as_query) {
                buffers_[0] = boost::asio::buffer(&status_, sizeof(status_));
                buffers_[1] = boost::asio::buffer(_view.pointer_, _view.size_);
                buffers_[2] = boost::asio::buffer(&ttl_type_, sizeof(ttl_type_));
                buffers_[3] = boost::asio::buffer(&ttl_, sizeof(ttl_));
                buffers_size_ = 4;
            } else {
                buffers_[0] = boost::asio::buffer(&status_, sizeof(status_));
                buffers_[1] = boost::asio::buffer(&ttl_type_, sizeof(ttl_type_));
                buffers_[2] = boost::asio::buffer(&ttl_, sizeof(ttl_));
                buffers_[3] = boost::asio::buffer(&value_size_, sizeof(value_size_));
                buffers_[4] = boost::asio::buffer(_view.pointer_, _view.size_);
                buffers_size_ = 5;
            }
        }

        /**
         * Constructor
         *
         * @param status_code
         */
        explicit response_holder(const uint8_t status_code)
        : status_(status_code) {
            buffers_[0] = boost::asio::buffer(&status_, sizeof(status_));
            buffers_size_ = 1;
        }
    };
}

#endif // THROTTR_RESPONSE_HPP
