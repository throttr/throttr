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

#include <throttr/request_header.hpp>

namespace throttr {
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
