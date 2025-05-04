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

#include <throttr/protocol.hpp>

#include <boost/asio/buffer.hpp>

namespace throttr {
    /**
     * Response holder
     */
    struct response_holder {
        /**
         * Buffers
         */
        std::vector<boost::asio::const_buffer> buffers_;

        /**
         * Status
         */
        std::uint8_t status_ = 0;

        /**
         * Quota
         */
        std::uint16_t quota_ = 0;

        /**
         * TTL type
         */
        std::uint8_t ttl_type_ = 0;

        /**
         * TTL
         */
        std::uint16_t ttl_ = 0;

        /**
         * Constructor
         *
         * @param e
         * @param ttl
         */
        response_holder(const request_entry &e, const std::uint16_t ttl)
            : status_(0x01),
              quota_(e.quota_),
              ttl_type_(static_cast<uint8_t>(e.ttl_type_)),
              ttl_(ttl) {
            buffers_ = {
                boost::asio::buffer(&status_, sizeof(status_)),
                boost::asio::buffer(&quota_, sizeof(quota_)),
                boost::asio::buffer(&ttl_type_, sizeof(ttl_type_)),
                boost::asio::buffer(&ttl_, sizeof(ttl_))
            };
        }

        /**
         * Constructor
         *
         * @param status_code
         */
        explicit response_holder(const uint8_t status_code)
            : status_(status_code) {
            buffers_ = {
                boost::asio::buffer(&status_, sizeof(status_))
            };
        }
    };
}

#endif // THROTTR_RESPONSE_HPP
