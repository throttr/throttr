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

#ifndef THROTTR_PROGRAM_PARAMETERS_HPP
#define THROTTR_PROGRAM_PARAMETERS_HPP

#include <string>

namespace throttr
{
  struct program_parameters
  {
    /**
     * Socket
     */
    std::string socket_ = "EMPTY";

    /**
     * Port
     */
    short port_ = 0;

    /**
     * Threads
     */
    int threads_ = 1;

    /**
     * As master
     */
    bool as_master_ = true;

    /**
     * Master host
     */
    std::string master_host_ = "localhost";

    /**
     * Master port
     */
    short master_port_ = 0;
  };
} // namespace throttr

#endif // THROTTR_PROGRAM_PARAMETERS_HPP
