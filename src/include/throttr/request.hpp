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
        std::variant<std::monostate, std::span<const uint8_t, 4>, std::span<const uint8_t, 16>> ip;
        uint16_t port = 0;
        std::string_view url;
        uint8_t max_requests = 0;
        uint32_t ttl_ms = 0;

        static request from_string(const std::string_view buffer) {
            request req;

            auto ptr = reinterpret_cast<const uint8_t*>(buffer.data());
            const uint8_t* end = ptr + buffer.size();

            if (ptr >= end) throw std::runtime_error("buffer too small");

            if (const uint8_t ip_version = *ptr++; ip_version == 4) {
                if (ptr + 4 > end) throw std::runtime_error("invalid IPv4 buffer");
                req.ip = std::span<const uint8_t, 4>(ptr, 4);
                ptr += 4;
            } else if (ip_version == 6) {
                if (ptr + 16 > end) throw std::runtime_error("invalid IPv6 buffer");
                req.ip = std::span<const uint8_t, 16>(ptr, 16);
                ptr += 16;
            } else {
                throw std::runtime_error("unsupported IP version");
            }

            if (ptr + 2 > end) throw std::runtime_error("missing port");
            req.port = (ptr[0] << 8) | ptr[1];
            ptr += 2;

            if (ptr >= end) throw std::runtime_error("missing URL length");
            const uint8_t url_len = *ptr++;
            if (ptr + url_len > end) throw std::runtime_error("URL out of bounds");
            req.url = std::string_view(reinterpret_cast<const char*>(ptr), url_len);
            ptr += url_len;

            if (ptr >= end) throw std::runtime_error("missing max_requests");
            req.max_requests = *ptr++;

            if (ptr + 4 > end) throw std::runtime_error("missing TTL");
            req.ttl_ms = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
            ptr += 4;

            return req;
        }

        [[nodiscard]] std::vector<uint8_t> to_bytes() const {
            std::vector<uint8_t> out;

            if (std::holds_alternative<std::span<const uint8_t, 4>>(ip)) {
                out.push_back(4);
                auto span = std::get<std::span<const uint8_t, 4>>(ip);
                out.insert(out.end(), span.begin(), span.end());
            } else if (std::holds_alternative<std::span<const uint8_t, 16>>(ip)) {
                out.push_back(6);
                auto span = std::get<std::span<const uint8_t, 16>>(ip);
                out.insert(out.end(), span.begin(), span.end());
            } else {
                throw std::runtime_error("IP not initialized");
            }

            out.push_back(static_cast<uint8_t>(port >> 8));
            out.push_back(static_cast<uint8_t>(port & 0xFF));

            if (url.size() > 255)
                throw std::runtime_error("URL too long");
            out.push_back(static_cast<uint8_t>(url.size()));
            out.insert(out.end(), url.begin(), url.end());

            out.push_back(max_requests);

            out.push_back(static_cast<uint8_t>((ttl_ms >> 24) & 0xFF));
            out.push_back(static_cast<uint8_t>((ttl_ms >> 16) & 0xFF));
            out.push_back(static_cast<uint8_t>((ttl_ms >> 8) & 0xFF));
            out.push_back(static_cast<uint8_t>(ttl_ms & 0xFF));

            return out;
        }
    };
}

#endif // THROTTR_REQUEST_HPP
