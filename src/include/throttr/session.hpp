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
#include <boost/beast/core/bind_handler.hpp>
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
         * @param state
         */
        explicit session(
            boost::asio::ip::tcp::socket socket,
            const std::shared_ptr<state> &state
        ) : socket_(std::move(socket)), state_(state) {
        }

        /**
         * Start
         */
        void start() {
            do_read();
        };

    private:
        /**
         * On read
         *
         * @param error
         * @param length
         */
        void on_read(const boost::system::error_code &error, const std::size_t length) {
            if (!error) {
                try {
                    const auto _buffer = std::span(reinterpret_cast<const std::byte *>(data_.data()), length);

                    if (_buffer.empty()) {
                        do_read();
                        return;
                    }

                    const auto _type = static_cast<request_type>(_buffer[0]);

                    std::vector<std::byte> _response;

                    if (_type == request_type::insert) {
                        const auto _request = request_insert::from_buffer(_buffer);
                        _response = state_->handle_insert(_request);
                    } else if (_type == request_type::query) {
                        const auto _request = request_query::from_buffer(_buffer);
                        _response = state_->handle_query(_request);
                    } else {
                        do_write({std::byte{0x00}});
                        return;
                    }

                    do_write(std::move(_response));
                } catch (const request_error &e) {
                    boost::ignore_unused(e);
                    do_write({std::byte{0x00}});
                }
            }
        }

        /**
         * Do read
         */
        void do_read() {
            socket_.async_read_some(boost::asio::buffer(data_.data(), data_.size()),
                                    boost::beast::bind_front_handler(&session::on_read, shared_from_this()));
        }

        /**
         * On write
         *
         * @param error
         * @param length
         */
        void on_write(const boost::system::error_code &error, const std::size_t length) {
            boost::ignore_unused(length);

            if (!error) {
                // LCOV_EXCL_LINE note: Partially tested as this requires a write error.
                do_read();
            }
        }

        /**
         * Do write
         *
         * @param response
         */
        void do_write(std::vector<std::byte> response) {
            auto _self(shared_from_this());
            boost::asio::async_write(socket_, boost::asio::buffer(response.data(), response.size()),
                                     boost::beast::bind_front_handler(&session::on_write, shared_from_this()));
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
        static constexpr std::size_t max_length_ = 1024;

        /**
         * Data
         */
        std::array<char, max_length_> data_{};
    };
}


#endif // THROTTR_SESSION_HPP
