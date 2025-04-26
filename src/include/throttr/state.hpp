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

#ifndef THROTTR_STATE_HPP
#define THROTTR_STATE_HPP

#include <throttr/request.hpp>

#include <memory>
#include <atomic>
#include <vector>
#include <unordered_map>

namespace throttr {
    /**
     * State
     */
    class state : public std::enable_shared_from_this<state> {
    public:
        /**
         * Acceptor ready
         */
        std::atomic_bool acceptor_ready_;

        /**
         * Requests
         */
        std::unordered_map<request_key, request_entry, request_key_hasher> requests_;

        /**
         * Handle request
         *
         * @param view
         * @return vector<byte>
         */
        std::vector<std::byte> handle_request(const request_view & view);
    };
}

#endif // THROTTR_STATE_HPP
