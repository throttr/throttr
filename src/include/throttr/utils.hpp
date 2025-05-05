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

#ifndef THROTTR_UTILS_HPP
#define THROTTR_UTILS_HPP

#include <throttr/protocol_wrapper.hpp>

// LCOV_EXCL_START
namespace throttr {
    /**
     * To string
     *
     * @param type
     * @return
     */
    inline std::string to_string(const ttl_types type) {
        using enum ttl_types;
        switch (type) {
            case nanoseconds: return "nanoseconds";
            case microseconds: return "microseconds";
            case milliseconds: return "milliseconds";
            case seconds: return "seconds";
            case minutes: return "minutes";
            case hours: return "hours";
            default: return "unknown";
        }
    }

    /**
     * To string
     *
     * @param type
     * @return
     */
    inline std::string to_string(const attribute_types type) {
        switch (type) {
            case attribute_types::quota: return "quota";
            case attribute_types::ttl: return "ttl";
            default: return "unknown";
        }
    }

    /**
     * To string
     *
     * @param type
     * @return
     */
    inline std::string to_string(const change_types type) {
        switch (type) {
            case change_types::increase: return "increase";
            case change_types::decrease: return "decrease";
            case change_types::patch: return "patch";
            default: return "unknown";
        }
    }
}
// LCOV_EXCL_STOP

#endif // THROTTR_UTILS_HPP
