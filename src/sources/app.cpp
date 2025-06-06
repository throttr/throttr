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

#include <throttr/app.hpp>

#include <thread>
#include <vector>

namespace throttr
{
  app::app(const program_parameters &program_options) : ioc_(program_options.threads_), program_options_(program_options)
  {
  }

  int app::serve()
  {
    std::remove(program_options_.socket_.c_str());

    server _server(ioc_, program_options_, state_);

    std::vector<std::jthread> _threads;
    _threads.reserve(program_options_.threads_);

    // LCOV_EXCL_START
    for (auto _i = program_options_.threads_; _i > 0; --_i)
    {
      _threads.emplace_back([self = shared_from_this()] { self->ioc_.run(); });
    }
    // LCOV_EXCL_STOP

    ioc_.run();

    // LCOV_EXCL_START
    for (auto &_thread : _threads)
    {
      _thread.join();
    }
    // LCOV_EXCL_STOP

    return EXIT_SUCCESS;
  }

  void app::stop()
  {
    ioc_.stop();
    std::remove(program_options_.socket_.c_str());
  }
} // namespace throttr