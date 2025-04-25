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

#include <boost/core/ignore_unused.hpp>
#include <throttr/request.hpp>

namespace throttr {
    struct request_error final : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    request request::from_string(const std::string_view buffer) {
        request req;

        auto ptr = reinterpret_cast<const uint8_t *>(buffer.data());
        const uint8_t *end = ptr + buffer.size();

        if (ptr >= end) {
            throw request_error("buffer too small");
        }

        if (const uint8_t ip_version = *ptr++; ip_version == 4) {
            if (ptr + 4 > end) {
                throw request_error("invalid IPv4 buffer");
            }
            req.ip = std::span<const uint8_t, 4>(ptr, 4);
            ptr += 4;
        } else if (ip_version == 6) {
            if (ptr + 16 > end) {
                throw request_error("invalid IPv6 buffer");
            }
            req.ip = std::span<const uint8_t, 16>(ptr, 16);
            ptr += 16;
        } else {
            throw request_error("unsupported IP version");
        }

        if (ptr + 2 > end) {
            throw request_error("missing port");
        }
        const auto b1 = static_cast<std::byte>(ptr[0]);
        const auto b2 = static_cast<std::byte>(ptr[1]);
        req.port = static_cast<uint16_t>(to_integer<uint8_t>(b1)) << 8
                   | static_cast<uint16_t>(to_integer<uint8_t>(b2));
        ptr += 2;

        if (ptr >= end) {
            throw request_error("missing URL length");
        }
        const uint8_t url_len = *ptr++;
        if (ptr + url_len > end) {
            throw request_error("URL out of bounds");
        }
        req.url = std::string_view(reinterpret_cast<const char *>(ptr), url_len);
        ptr += url_len;

        if (ptr >= end) {
            throw request_error("missing max_requests");
        }
        req.max_requests = *ptr++;

        if (ptr + 4 > end) {
            throw request_error("missing TTL");
        }

        const auto c0 = static_cast<std::byte>(ptr[0]);
        const auto c1 = static_cast<std::byte>(ptr[1]);
        const auto c2 = static_cast<std::byte>(ptr[2]);
        const auto c3 = static_cast<std::byte>(ptr[3]);

        req.ttl_ms =
                static_cast<uint32_t>(to_integer<uint8_t>(c0)) << 24 |
                static_cast<uint32_t>(to_integer<uint8_t>(c1)) << 16 |
                static_cast<uint32_t>(to_integer<uint8_t>(c2)) << 8 |
                static_cast<uint32_t>(to_integer<uint8_t>(c3));

        ptr += 4;

        return req;
    }

    std::vector<uint8_t> request::to_bytes() const {
        std::vector<uint8_t> out;

        if (std::holds_alternative<std::span<const uint8_t, 4> >(ip)) {
            out.push_back(4);
            auto span = std::get<std::span<const uint8_t, 4> >(ip);
            out.insert(out.end(), span.begin(), span.end());
        } else if (std::holds_alternative<std::span<const uint8_t, 16> >(ip)) {
            out.push_back(6);
            auto span = std::get<std::span<const uint8_t, 16> >(ip);
            out.insert(out.end(), span.begin(), span.end());
        } else {
            throw request_error("IP not initialized");
        }

        out.push_back(static_cast<uint8_t>(port >> 8));
        out.push_back(static_cast<uint8_t>(port & 0xFF));

        if (url.size() > 255)
            throw request_error("URL too long");
        out.push_back(static_cast<uint8_t>(url.size()));
        out.insert(out.end(), url.begin(), url.end());

        out.push_back(max_requests);

        out.push_back(static_cast<uint8_t>((ttl_ms >> 24) & 0xFF));
        out.push_back(static_cast<uint8_t>((ttl_ms >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((ttl_ms >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>(ttl_ms & 0xFF));

        return out;
    }
}
