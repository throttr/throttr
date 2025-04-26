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

#ifndef THROTTR_SESSION_HPP
#define THROTTR_SESSION_HPP

#include <memory>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/core/ignore_unused.hpp>
#include <throttr/request.hpp>
#include <throttr/state.hpp>

namespace throttr {
    /**
     * Session
     */
    class session : public std::enable_shared_from_this<session> {
    public:
        /**
         * Constructor
         *
         * @param socket
         */
        explicit session(
            boost::asio::ip::tcp::socket socket,
            const std::shared_ptr<state> & state
        );

        /**
         * Start
         */
        void start();

    private:
        /**
         * Do read
         */
        void do_read() {
            auto _self(shared_from_this());
            socket_.async_read_some(boost::asio::buffer(data_.data(), data_.size()),
                                    [this, _self](const boost::system::error_code &error,
                                                 const std::size_t read_length) {
                                        boost::ignore_unused(_self);
                                        if (!error) {
                                            try {
                                                const auto _view = request_view::from_buffer(
                                                    std::span(
                                                        reinterpret_cast<const std::byte*>(data_.data()),
                                                        read_length
                                                    )
                                                );

                                                const auto _response = state_->handle_request(_view);

                                                do_write(_response);
                                            } catch (const request_error& e) {
                                                do_read();
                                            }
                                        }
                                    });
        }

        /**
         * Do write
         *
         * @param response
         */
        void do_write(std::vector<std::byte> response) {
            auto _self(shared_from_this());
            boost::asio::async_write(socket_, boost::asio::buffer(response.data(), response.size()),
                                     [this, _self](const boost::system::error_code &error,
                                                  const std::size_t write_length) {
                                         boost::ignore_unused(_self, write_length);

                                         if (!error) {
                                             do_read();
                                         }
                                     });
        }

        /**
         * Socket
         */
        boost::asio::ip::tcp::socket socket_;

        /**
         * State
         */
        std::shared_ptr<state> state_;

        /**
         * Max length
         */
        static constexpr std::size_t max_length = 1024;

        /**
         * Data
         */
        std::array<char, max_length> data_{};
    };
}


#endif // THROTTR_SESSION_HPP
