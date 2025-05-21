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

#include <throttr/server.hpp>
#include <throttr/state.hpp>

#include <thread>
#include <vector>

namespace throttr {
/**
 * App
 */
class app : public std::enable_shared_from_this<app> {
 public:
  /**
   * IO Context
   */
  boost::asio::io_context ioc_;

  /**
   * Port
   */
  short port_;

  /**
   * Threads
   */
  int threads_;

  /**
   * State
   */
  std::shared_ptr<state> state_ = std::make_shared<state>(ioc_);

  /**
   * Construct
   *
   * @param port
   * @param threads
   */
  explicit app(const short port, const int threads)
      : ioc_(threads), port_(port), threads_(threads) {}

  /**
   * Serve
   *
   * @return int
   */
  int serve() {
    server _server(ioc_, port_, state_);

    std::vector<std::jthread> _threads;
    _threads.reserve(threads_);

    // LCOV_EXCL_START
    for (auto _i = threads_; _i > 0; --_i) {
      _threads.emplace_back([self = shared_from_this()] { self->ioc_.run(); });
    }
    // LCOV_EXCL_STOP

    ioc_.run();

    // LCOV_EXCL_START
    for (auto& _thread : _threads) {
      _thread.join();
    }
    // LCOV_EXCL_STOP

    return EXIT_SUCCESS;
  }

  /**
   * Stop
   *
   * @return void
   */
  void stop() { ioc_.stop(); }
};
}  // namespace throttr

#endif  // THROTTR_APP_HPP
