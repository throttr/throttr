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
            boost::asio::ip::tcp::socket socket
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
            auto self(shared_from_this());
            socket_.async_read_some(boost::asio::buffer(data_, max_length),
                                    [this, self](const boost::system::error_code &error,
                                                 const std::size_t read_length) {
                                        boost::ignore_unused(self);

                                        if (!error) {
                                            do_write(read_length);
                                        }
                                    });
        }

        /**
         * Do write
         *
         * @param length
         */
        void do_write(const std::size_t length) {
            auto self(shared_from_this());
            boost::asio::async_write(socket_, boost::asio::buffer(data_, length),
                                     [this, self](const boost::system::error_code &error,
                                                  const std::size_t write_length) {
                                         boost::ignore_unused(self, write_length);

                                         if (!error) {
                                             do_read();
                                         }
                                     });
        }

        boost::asio::ip::tcp::socket socket_;

        enum { max_length = 1024 };

        char data_[max_length] = "\0";
    };
}


#endif // THROTTR_SESSION_HPP
