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
    struct response_holder {
        std::vector<boost::asio::const_buffer> buffers;

        std::uint32_t request_id_ = 0;
        std::uint8_t status_ = 0;
        std::uint64_t quota_ = 0;
        std::uint8_t ttl_type_ = 0;
        std::uint64_t ttl_remaining_ = 0;

        response_holder(const request_query &request, const request_entry &e, const std::uint64_t ttl)
            : request_id_(request.header_->request_id_),
              status_(0x01),
              quota_(e.quota_),
              ttl_type_(static_cast<uint8_t>(e.ttl_type_)),
              ttl_remaining_(ttl) {
            buffers = {
                boost::asio::buffer(&request_id_, sizeof(request_id_)),
                boost::asio::buffer(&status_, sizeof(status_)),
                boost::asio::buffer(&quota_, sizeof(quota_)),
                boost::asio::buffer(&ttl_type_, sizeof(ttl_type_)),
                boost::asio::buffer(&ttl_remaining_, sizeof(ttl_remaining_))
            };
        }

        response_holder(const uint32_t request_id, const uint8_t status_code)
            : request_id_(request_id), status_(status_code) {
            buffers = {
                boost::asio::buffer(&request_id_, sizeof(request_id_)),
                boost::asio::buffer(&status_, sizeof(status_))
            };
        }
    };
}

#endif // THROTTR_RESPONSE_HPP
