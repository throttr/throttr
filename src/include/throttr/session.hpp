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

#include <throttr/state.hpp>
#include <throttr/session_memory.hpp>

#include <throttr/protocol_wrapper.hpp>

#include <deque>
#include <memory>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/asio/bind_allocator.hpp>

#include <fmt/chrono.h>

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
            // LCOV_EXCL_START
            if (socket_.is_open()) {
                const boost::asio::ip::tcp::no_delay no_delay_option(true);
                socket_.set_option(no_delay_option);
#ifndef NDEBUG
                ip_ = socket_.remote_endpoint().address().to_string();
                port_ = socket_.remote_endpoint().port();
#endif
            }
            // LCOV_EXCL_STOP
        }

        /**
         * Destructor
         */
        ~session() {
            // LCOV_EXCL_START
#ifndef NDEBUG
            fmt::println("{:%Y-%m-%d %H:%M:%S} SESSION CLOSED ip={} port={}", std::chrono::system_clock::now(), ip_, port_);
#endif
            // LCOV_EXCL_STOP
        }

        /**
         * Start
         */
        void start() {
            // LCOV_EXCL_START
#ifndef NDEBUG
            fmt::println("{:%Y-%m-%d %H:%M:%S} SESSION ESTABLISHED ip={} port={}", std::chrono::system_clock::now(), ip_, port_);
#endif
            // LCOV_EXCL_STOP
            do_read();
        }

        /**
         * Compact buffer if needed
         */
        void compact_buffer_if_needed() {
            if (buffer_start_ == buffer_end_) { // LCOV_EXCL_LINE note: Partially tested.
                buffer_start_ = 0;
                buffer_end_ = 0;
            } else if (buffer_start_ > max_length_ / 2) { // LCOV_EXCL_LINE note: Partially tested.
                compact_buffer();
            }
        }

        /**
         * Compact buffer
         */
        void compact_buffer() {
            if (buffer_start_ == buffer_end_) return; // LCOV_EXCL_LINE note: Partially tested.
            std::memmove(buffer_.data(), buffer_.data() + buffer_start_, buffer_end_ - buffer_start_);
            buffer_end_ -= buffer_start_;
            buffer_start_ = 0;
        }

        /**
         * Max length
         */
        static constexpr std::size_t max_length_ = 4096;

        /**
         * Buffer
         */
        std::array<std::byte, max_length_> buffer_;


        /**
         * Buffer start
         */
        std::size_t buffer_start_ = 0;

        /**
         * Buffer end
         */
        std::size_t buffer_end_ = 0;

#ifndef NDEBUG
        /**
         * IP
         */
        std::string ip_ = "127.0.0.1";

        /**
         * Port
         */
        std::uint16_t port_ = 13579;
#endif
    private:
        /**
         * Handler memory
         */
        handler_memory handler_memory_;

        /**
         * On read
         *
         * @param error
         * @param length
         */
        void on_read(const boost::system::error_code &error, const std::size_t length) {
            if (!error) { // LCOV_EXCL_LINE note: Partially tested.
                buffer_end_ += length;
                try_process_next();
                // LCOV_EXCL_START
            } else {
                close_socket();
            }
            // LCOV_EXCL_STOP
        }

        /**
         * Try process next
         */
        void try_process_next() {
            while (true) {
                std::span<const std::byte> _span(buffer_.data() + buffer_start_, buffer_end_ - buffer_start_);
                const std::size_t _msg_size = get_message_size(_span);
                if (_msg_size == 0 || _span.size() < _msg_size) break; // LCOV_EXCL_LINE note: Ignored.

                std::span<const std::byte> _view(buffer_.data() + buffer_start_, _msg_size);
                buffer_start_ += _msg_size;

                // LCOV_EXCL_START
#ifndef NDEBUG
                fmt::println("{:%Y-%m-%d %H:%M:%S} SESSION READ ip={} port={} buffer={}", std::chrono::system_clock::now(), ip_, port_, span_to_hex(_view));
#endif
            // LCOV_EXCL_STOP

                auto _response = response_holder(0x00);

                try {
                    switch (const auto type = static_cast<request_types>(std::to_integer<uint8_t>(_view[0])); type) {
                        case request_types::insert:
                            _response = state_->handle_insert(_view);
                            break;
                        case request_types::set:
                            _response = state_->handle_set(_view);
                            break;
                        case request_types::query:
                        case request_types::get:
                            _response = state_->handle_query(request_query::from_buffer(_view), type == request_types::query);
                            break;
                        case request_types::update:
                            _response = state_->handle_update(request_update::from_buffer(_view));
                            break;
                        case request_types::purge:
                            _response = state_->handle_purge(request_purge::from_buffer(_view));
                            break;
                        // LCOV_EXCL_START
                    }
                } catch (const request_error &e) {
                    boost::ignore_unused(e);
                }
                // LCOV_EXCL_STOP

                write_queue_.emplace_back(_response);
            }

            // LCOV_EXCL_START Note: Partially tested.
            // The not tested case is when in-while break condition is triggered but no queue element exists.
            do_write();
            // LCOV_EXCL_STOP

            compact_buffer_if_needed();
        }

        /**
         * Do write
         */
        void do_write() {
            // LCOV_EXCL_START
            if (write_queue_.empty()) {
                do_read();
                return;
            }
            // LCOV_EXCL_STOP

            std::vector<boost::asio::const_buffer> _batch;

            _batch.reserve(write_queue_.size() * 5);

            // LCOV_EXCL_START Note: Partially tested.
            // The not tested case TBC is when execution reach this code but the queue or buffers are empty.
            for (const auto& _response : write_queue_) {
                for (const auto& _buffer : _response.buffers_) {
                    if (boost::asio::buffer_size(_buffer) > 0) _batch.emplace_back(_buffer);
                }
            }
            // LCOV_EXCL_STOP

            auto self = shared_from_this();

            // LCOV_EXCL_START
#ifndef NDEBUG
            fmt::println("{:%Y-%m-%d %H:%M:%S} SESSION WRITE ip={} port={} buffer={}", std::chrono::system_clock::now(), ip_, port_, buffers_to_hex(_batch));
#endif
            // LCOV_EXCL_STOP

            boost::asio::async_write(
                socket_,
                    _batch,
                    boost::asio::bind_allocator(
                        handler_allocator<int>(handler_memory_),
                        [self] (const boost::system::error_code& ec, const std::size_t length) {
                            self->on_write(ec, length);
                        }
                    )
                );
        }

        /**
         * Get message size
         *
         * @param buffer
         * @return
         */
        static std::size_t get_message_size(std::span<const std::byte> buffer) {
            if (buffer.empty()) return 0;

            const auto *_buffer = buffer.data();
            const auto _type = static_cast<request_types>(std::to_integer<uint8_t>(_buffer[0])); // NOSONAR

            auto _check_and_return = [&](const std::size_t header_size, const std::size_t extra = 0) -> std::size_t {
                if (buffer.size() < header_size) return 0;
                if (buffer.size() < header_size + extra) return 0;
                return header_size + extra;
            };

            switch (_type) {
                case request_types::insert: {
                    auto *_h = reinterpret_cast<const request_insert_header *>(_buffer); // NOSONAR
                    return _check_and_return(request_insert_header_size, _h->key_size_);
                }
                case request_types::query: {
                    auto *_h = reinterpret_cast<const request_query_header *>(_buffer); // NOSONAR
                    return _check_and_return(request_query_header_size, _h->key_size_);
                }
                case request_types::update: {
                    auto *_h = reinterpret_cast<const request_update_header *>(_buffer); // NOSONAR
                    return _check_and_return(request_update_header_size, _h->key_size_);
                }
                case request_types::purge: {
                    auto *_h = reinterpret_cast<const request_purge_header *>(_buffer); // NOSONAR
                    return _check_and_return(request_purge_header_size, _h->key_size_);
                }
                case request_types::set: {
                    auto *_h = reinterpret_cast<const request_set_header *>(_buffer); // NOSONAR
                    return _check_and_return(request_set_header_size, _h->key_size_ + _h->value_size_);
                }
                case request_types::get: {
                    auto *_h = reinterpret_cast<const request_get_header *>(_buffer); // NOSONAR
                    return _check_and_return(request_get_header_size, _h->key_size_);
                }
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
            // LCOV_EXCL_START
            if (buffer_end_ == max_length_) {
                compact_buffer();
            }
            // LCOV_EXCL_STOP

            auto self = shared_from_this();
            socket_.async_read_some(
                boost::asio::buffer(buffer_.data() + buffer_end_, max_length_ - buffer_end_),
                boost::asio::bind_allocator(
                        handler_allocator<int>(handler_memory_),
                        [self] (const boost::system::error_code& ec, const std::size_t length) {
                            self->on_read(ec, length);
                        }
                    )
            );
        }

        /**
         * On write
         *
         * @param error
         * @param length
         */
        void on_write(const boost::system::error_code &error, const std::size_t length) {
            boost::ignore_unused(length);

            while (!write_queue_.empty()) write_queue_.pop_front(); // LCOV_EXCL_LINE Note: Partially tested
            // The not tested case is when write queue is empty
            // which isn't possible if the previous sequence was writing.

            // LCOV_EXCL_START
            if (error) {
                close_socket();
                return;
            }
            // LCOV_EXCL_STOP

            do_read();
        }

        void close_socket() {
            boost::system::error_code ec;
            socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            socket_.close(ec);
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
         * Write queue
         */
        std::deque<response_holder> write_queue_;
    };
}


#endif // THROTTR_SESSION_HPP
