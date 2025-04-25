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

namespace throttr {
    app::app(
        const short port,
        const int threads
    ) : ioc_(threads),
        port_(port),
        threads_(threads) {
    }

    int app::serve() {
        server _server(
            ioc_,
            port_,
            state_
        );

        std::vector<std::jthread> _threads;
        _threads.reserve(threads_);

        // LCOV_EXCL_START
        for (auto _i = threads_; _i > 0; --_i) {
            _threads.emplace_back([self = shared_from_this()] {
                self->ioc_.run();
            });
        }
        // LCOV_EXCL_STOP

        ioc_.run();

        // LCOV_EXCL_START
        for (auto &_thread: _threads) {
            _thread.join();
        }
        // LCOV_EXCL_STOP

        return EXIT_SUCCESS;
    }

    void app::stop() {
        ioc_.stop();
    }
}
