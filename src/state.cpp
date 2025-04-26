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

#include <throttr/state.hpp>
#include <cstring>

namespace throttr {
    std::vector<std::byte> state::handle_request(
        const request_view &view) {
        const auto* header = view.header_;

        const request_key key{
            header->ip_version_,
            header->ip_,
            header->port_,
            std::string(view.url_)
        };

        const auto now = std::chrono::steady_clock::now();
        auto it = requests_.find(key);

        if (it != requests_.end() && now >= it->second.expires_at) {
            requests_.erase(it);
            it = requests_.end();
        }

        bool can = false;
        int available = 0;
        int64_t ttl_ms = 0;

        if (it != requests_.end()) {
            auto& entry = it->second;

            if (entry.available_requests > 0) {
                --entry.available_requests;
                can = true;
            }
            available = entry.available_requests;
            ttl_ms = std::chrono::duration_cast<std::chrono::milliseconds>(entry.expires_at - now).count();
        } else {
            const int stock = static_cast<int>(header->max_requests_);
            const int ttl = static_cast<int>(header->ttl_);

            requests_[key] = request_entry{
                stock - 1,
                now + std::chrono::milliseconds(ttl)
            };

            can = true;
            available = stock - 1;
            ttl_ms = ttl;
        }

        std::vector<std::byte> response;
        response.resize(1 + sizeof(int) + sizeof(int64_t));

        response[0] = static_cast<std::byte>(can ? 1 : 0);
        std::memcpy(response.data() + 1, &available, sizeof(available));
        std::memcpy(response.data() + 1 + sizeof(int), &ttl_ms, sizeof(ttl_ms));

        return response;
    }
}
