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

#ifndef THROTTR_VERSION_HPP
#define THROTTR_VERSION_HPP

#include <string_view>

namespace throttr {
    /**
     * Version
     */
    constexpr std::string_view version = "1.0.0";

    /**
     * Get version
     *
     * @return string_view
     */
    std::string_view get_version();
}

#endif // THROTTR_VERSION_HPP
