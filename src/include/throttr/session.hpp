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

#include <deque>
#include <iomanip>
#include <iostream>
#include <memory>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <throttr/state.hpp>
#include <throttr/protocol.hpp>

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
        }

    private:
        /**
         * On read
         *
         * @param error
         * @param length
         */
        void on_read(const boost::system::error_code &error, const std::size_t length) {
            if (!error) { // LCOV_EXCL_LINE note: Partially tested.
                buffer_.insert(buffer_.end(),
                               reinterpret_cast<const std::byte *>(data_.data()),
                               reinterpret_cast<const std::byte *>(data_.data() + length));
                try_process_next();
                // LCOV_EXCL_START
            } else {
                boost::system::error_code ec;
                socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                socket_.close(ec);
            }
            // LCOV_EXCL_STOP
        }

        /**
         * Try process next
         */
        void try_process_next() {
            while (true) {
                if (buffer_.empty()) break;

                const std::span<const std::byte> span(buffer_.data(), buffer_.size());
                const std::size_t msg_size = get_message_size(span);

                if (msg_size == 0 || buffer_.size() < msg_size) break;

                std::span<const std::byte> view(buffer_.data(), msg_size);
                buffer_.erase(buffer_.begin(), std::next(buffer_.begin(), static_cast<std::ptrdiff_t>(msg_size)));

                std::vector response = {std::byte{0x00}};

                try {
                    switch (const auto type = static_cast<request_types>(std::to_integer<uint8_t>(view[0]))) {
                        case request_types::insert:
                            response = state_->handle_insert(request_insert::from_buffer(view));
                            break;
                        case request_types::query:
                            response = state_->handle_query(request_query::from_buffer(view));
                            break;
                        case request_types::update:
                            response = state_->handle_update(request_update::from_buffer(view));
                            break;
                        case request_types::purge:
                            response = state_->handle_purge(request_purge::from_buffer(view));
                            break;
                        // LCOV_EXCL_START
                        default:
                            response = {std::byte{0x00}};
                            break;
                    }
                } catch (const request_error &e) {
                    boost::ignore_unused(e);
                }
                // LCOV_EXCL_STOP

                const bool queue_was_empty = write_queue_.empty();
                write_queue_.emplace_back(std::move(response));

                if (queue_was_empty) {
                    do_write();
                }
            }

            if (write_queue_.empty()) {
                do_read();
            }
        }

        /**
         * Do write
         */
        void do_write() {
            auto &msg = write_queue_.front();
            auto self = shared_from_this();
            boost::asio::async_write(socket_, boost::asio::buffer(msg.data(), msg.size()),
                                     boost::beast::bind_front_handler(&session::on_write, self));
        }

        /**
         * Get message size
         *
         * @param buffer
         * @return
         */
        static std::size_t get_message_size(std::span<const std::byte> buffer) {
            if (buffer.empty()) return 0; // LCOV_EXCL_LINE note: Ignored.

            switch (static_cast<request_types>(std::to_integer<uint8_t>(buffer[0]))) {
                case request_types::insert:
                    if (buffer.size() < request_insert_header_size) return 0; // LCOV_EXCL_LINE note: Ignored.
                    return request_insert_header_size
                           + reinterpret_cast<const request_insert_header *>(buffer.data())->consumer_id_size_
                           + reinterpret_cast<const request_insert_header *>(buffer.data())->resource_id_size_;
                case request_types::query:
                    if (buffer.size() < request_query_header_size) return 0; // LCOV_EXCL_LINE note: Ignored.
                    return request_query_header_size
                           + reinterpret_cast<const request_query_header *>(buffer.data())->consumer_id_size_
                           + reinterpret_cast<const request_query_header *>(buffer.data())->resource_id_size_;
                case request_types::update:
                    if (buffer.size() < request_update_header_size) return 0; // LCOV_EXCL_LINE note: Ignored.
                    return request_update_header_size
                           + reinterpret_cast<const request_update_header *>(buffer.data())->consumer_id_size_
                           + reinterpret_cast<const request_update_header *>(buffer.data())->resource_id_size_;
                case request_types::purge:
                    if (buffer.size() < request_purge_header_size) return 0; // LCOV_EXCL_LINE note: Ignored.
                    return request_purge_header_size
                           + reinterpret_cast<const request_purge_header *>(buffer.data())->consumer_id_size_
                           + reinterpret_cast<const request_purge_header *>(buffer.data())->resource_id_size_;
                // LCOV_EXCL_START
                default:
                    return 0;
                // LCOV_EXCL_STOP
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

            write_queue_.pop_front();

            // LCOV_EXCL_START
            if (error) {
                boost::system::error_code ec;
                socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                socket_.close(ec);
                return;
            }
            // LCOV_EXCL_STOP


            if (!write_queue_.empty()) {
                // LCOV_EXCL_LINE note: Partially tested.
                do_write(); // LCOV_EXCL_LINE note: Ignored.
            } else {
                do_read();
            }
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

        /**
         * Buffer
         */
        std::vector<std::byte> buffer_;

        /**
         * Write queue
         */
        std::deque<std::vector<std::byte> > write_queue_;
    };
}


#endif // THROTTR_SESSION_HPP
