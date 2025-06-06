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

#ifndef THROTTR_APP_HPP
#define THROTTR_APP_HPP

#include <throttr/program_parameters.hpp>
#include <throttr/server.hpp>
#include <throttr/state.hpp>

namespace throttr
{
  /**
   * App
   */
  class app : public std::enable_shared_from_this<app>
  {
  public:
    /**
     * IO Context
     */
    boost::asio::io_context ioc_;

    /**
     * Program options
     */
    program_parameters program_options_;

    /**
     * State
     */
    std::shared_ptr<state> state_ = std::make_shared<state>(ioc_);

    /**
     * Construct
     *
     * @param program_options
     * @param threads
     */
    explicit app(const program_parameters &program_options);

    /**
     * Serve
     *
     * @return int
     */
    int serve();

    /**
     * Stop
     *
     * @return void
     */
    void stop();
  };
} // namespace throttr

#endif // THROTTR_APP_HPP
