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
        const auto* _header = view.header_;

        const request_key key{
            _header->ip_version_,
            _header->ip_,
            _header->port_,
            std::string(view.url_)
        };

        const auto _now = std::chrono::steady_clock::now();
        auto _it = requests_.find(key);

        if (_it != requests_.end() && _now >= _it->second.expires_at_) {
            requests_.erase(_it);
            _it = requests_.end();
        }

        bool _can = false;
        int _available = 0;
        int64_t _default_ttl = 0;

        if (_it != requests_.end()) {
            auto& _entry = _it->second;

            if (_entry.available_requests_ > 0) {
                --_entry.available_requests_;
                _can = true;
            }
            _available = _entry.available_requests_;
            _default_ttl = std::chrono::duration_cast<std::chrono::milliseconds>(_entry.expires_at_ - _now).count();
        } else {
            const int _stock = static_cast<int>(_header->max_requests_);
            const int _ttl = static_cast<int>(_header->ttl_);

            requests_[key] = request_entry{
                _stock - 1,
                _now + std::chrono::milliseconds(_ttl)
            };

            _can = true;
            _available = _stock - 1;
            _default_ttl = _ttl;
        }

        std::vector<std::byte> _response;
        _response.resize(1 + sizeof(int) + sizeof(int64_t));

        _response[0] = static_cast<std::byte>(_can ? 1 : 0);
        std::memcpy(_response.data() + 1, &_available, sizeof(_available));
        std::memcpy(_response.data() + 1 + sizeof(int), &_default_ttl, sizeof(_default_ttl));

        return _response;
    }
}
