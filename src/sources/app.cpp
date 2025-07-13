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
#include <throttr/message.hpp>

#include <boost/asio/signal_set.hpp>

#include <throttr/buffers_pool.hpp>
#include <throttr/messages_pool.hpp>

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

    const auto _agent = std::make_shared<agent>(ioc_, program_options_, state_);
    _agent->start();

    std::vector<std::jthread> _threads;
    _threads.reserve(program_options_.threads_);

    for (auto _i = program_options_.threads_; _i > 0; --_i)
    {
      _threads.emplace_back([self = shared_from_this(), state = state_->shared_from_this()] { self->boot(); });
    }

    state_->prepare_for_startup(program_options_);

    boost::asio::signal_set signals(ioc_, SIGINT, SIGTERM);
    signals.async_wait(
      [&](auto /*ec*/, int /*signal_number*/)
      {
        fmt::println("[{}] [{:%Y-%m-%d %H:%M:%S}] SIGNAL RECEIVED", to_string(state_->id_), std::chrono::system_clock::now());
        state_->prepare_for_shutdown(program_options_);
        ioc_.stop();
      });

    boot();

    for (auto &_thread : _threads)
    {
      _thread.join();
    }

    return EXIT_SUCCESS;
  }

  void app::stop()
  {
    ioc_.stop();
    std::remove(program_options_.socket_.c_str());
  }

  void app::boot()
  {
    messages_pool::prepares();
    buffers_pool::prepares();

    ioc_.run();
  }
} // namespace throttr