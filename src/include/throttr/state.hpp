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

#ifndef THROTTR_STATE_HPP
#define THROTTR_STATE_HPP

#include <throttr/protocol.hpp>
#include <throttr/storage.hpp>
#include <throttr/time.hpp>

#include <memory>
#include <atomic>
#include <vector>
#include <cstring>
#include <deque>
#include <boost/core/ignore_unused.hpp>
#include <boost/asio/strand.hpp>

namespace throttr {
    /**
     * State
     */
    class state : public std::enable_shared_from_this<state> {
    public:
        /**
         * Acceptor ready
         */
        std::atomic_bool acceptor_ready_;


        /**
         * Storage
         */
        storage_type storage_;

        /**
         * Expiration timer
         */
        boost::asio::steady_timer expiration_timer_;

        /**
         * Strand
         */
        boost::asio::strand<boost::asio::io_context::executor_type> strand_;

        /**
         * Constructor
         *
         * @param ioc
         */
        explicit state(boost::asio::io_context &ioc) : expiration_timer_(ioc), strand_(ioc.get_executor()) {
        }


        /**
         * Handle insert
         *
         * @param request
         * @return std::vector<std::byte>
         */
        std::vector<std::byte> handle_insert(const request_insert &request) {
            const request_key _key{std::string(request.consumer_id_), std::string(request.resource_id_)};
            const auto _now = std::chrono::steady_clock::now();
            const auto _expires_at = get_expiration_point(_now, request.header_->ttl_type_, request.header_->ttl_);

            auto &_index = storage_.get<tag_by_expiration>();

            auto [_, _inserted] = storage_.insert({
                _key,
                {request.header_->quota_, request.header_->ttl_type_, _expires_at}
            });

            boost::ignore_unused(_);

            if (_inserted) {
                if (const auto &_entry = _index.begin()->entry_; _expires_at <= _entry.expires_at_) {
                    boost::asio::post(strand_, [_self = shared_from_this(), _expires_at] {
                        _self->schedule_expiration(_expires_at);
                    });
                }
            }

            constexpr std::size_t _offset_response_request_id = 0;
            constexpr std::size_t _offset_response_status = _offset_response_request_id + sizeof(uint32_t);
            constexpr std::size_t _response_size = _offset_response_status + sizeof(uint8_t);

            std::vector<std::byte> _response(_response_size);

            std::memcpy(_response.data() + _offset_response_request_id, &request.header_->request_id_,
                        sizeof(uint32_t));
            _response[_offset_response_status] = static_cast<std::byte>(_inserted);


            return _response;
        }

        /**
         * Handle query
         *
         * @param request
         * @return std::vector<std::byte>
         */
        std::vector<std::byte> handle_query(const request_query &request) {
            const request_key _key{std::string(request.consumer_id_), std::string(request.resource_id_)};
            const auto _now = std::chrono::steady_clock::now();

            auto &_index = storage_.get<tag_by_key>();
            const auto _it = _index.find(_key);

            constexpr std::size_t _offset_response_request_id = 0;
            constexpr std::size_t _offset_response_status = _offset_response_request_id + sizeof(uint32_t);
            constexpr std::size_t _response_error_size = _offset_response_status + sizeof(uint8_t);
            constexpr std::size_t _offset_response_quota = _offset_response_status + sizeof(uint8_t);
            constexpr std::size_t _offset_response_ttl_type = _offset_response_quota + sizeof(uint64_t);
            constexpr std::size_t _offset_response_ttl = _offset_response_ttl_type + sizeof(uint8_t);
            constexpr std::size_t _response_success_size = _offset_response_ttl + sizeof(uint64_t);

            if (_it == _index.end() || _now >= _it->entry_.expires_at_) {
                std::vector<std::byte> _response(_response_error_size);
                std::memcpy(_response.data() + _offset_response_request_id, &request.header_->request_id_,
                            sizeof(uint32_t));
                _response[_offset_response_status] = std::byte{0x00};
                return _response;
            }

            const auto &_entry = _it->entry_;
            const auto _ttl_remaining = get_ttl(_entry.expires_at_, _entry.ttl_type_);

            std::vector<std::byte> _response(_response_success_size);

            std::memcpy(_response.data() + _offset_response_request_id, &request.header_->request_id_,
                        sizeof(uint32_t));
            _response[_offset_response_status] = std::byte{0x01};
            std::memcpy(_response.data() + _offset_response_quota, &_entry.quota_, sizeof(uint64_t));
            _response[_offset_response_ttl_type] = static_cast<std::byte>(_entry.ttl_type_);
            std::memcpy(_response.data() + _offset_response_ttl, &_ttl_remaining, sizeof(uint64_t));

            return _response;
        }

        /**
         * Handle update
         *
         * @param request
         * @return std::vector<std::byte>
         */
        std::vector<std::byte> handle_update(const request_update &request) {
            const request_key _key{std::string(request.consumer_id_), std::string(request.resource_id_)};
            const auto _now = std::chrono::steady_clock::now();

            auto &_index = storage_.get<tag_by_key>();
            const auto _it = _index.find(_key);

            constexpr std::size_t _offset_id = 0;
            constexpr std::size_t _offset_status = _offset_id + sizeof(uint32_t);
            constexpr std::size_t _response_size = _offset_status + sizeof(uint8_t);

            std::vector<std::byte> _response(_response_size);
            std::memcpy(_response.data() + _offset_id, &request.header_->request_id_, sizeof(uint32_t));

            if (_it == _index.end() || _now >= _it->entry_.expires_at_) {
                _response[_offset_status] = std::byte{0x00};
                return _response;
            }

            using enum attribute_types;

            const bool _was_modified = storage_.modify(_it, [&](entry_wrapper & object) {
                switch (request.header_->attribute_) {
                    case quota:
                        apply_quota_change(object.entry_, request);
                        break;
                    case ttl:
                        apply_ttl_change(object.entry_, request, _now);
                        break;
                }
            });

            _response[_offset_status] = std::byte{_was_modified};
            return _response;
        }

        /**
         * Apply quota change
         *
         * @param entry
         * @param request
         * @return bool
         */
        static bool apply_quota_change(request_entry &entry, const request_update &request) {
            using enum change_types;

            switch (request.header_->change_) {
                case patch:
                    entry.quota_ = request.header_->value_;
                    break;
                case increase:
                    entry.quota_ += request.header_->value_;
                    break;
                case decrease:
                    if (entry.quota_ >= request.header_->value_) {
                        entry.quota_ -= request.header_->value_;
                    } else {
                        entry.quota_ = 0;
                    }
                    break;
            }
            return true;
        }

        /**
         * Apply TTL change
         *
         * @param entry
         * @param request
         * @param _now
         * @return bool
         */
        static bool apply_ttl_change(request_entry &entry, const request_update &request,
                                     const std::chrono::steady_clock::time_point &_now) {
            using enum ttl_types;
            std::chrono::nanoseconds _duration;

            switch (entry.ttl_type_) {
                case seconds:
                    _duration = std::chrono::seconds(request.header_->value_);
                    break;
                case milliseconds:
                    _duration = std::chrono::milliseconds(request.header_->value_);
                    break;
                case nanoseconds:
                default:
                    _duration = std::chrono::nanoseconds(request.header_->value_);
                    break;
            }

            switch (request.header_->change_) {
                case change_types::patch:
                    entry.expires_at_ = _now + _duration;
                    break;
                case change_types::increase:
                    entry.expires_at_ += _duration;
                    break;
                case change_types::decrease:
                    entry.expires_at_ -= _duration;
                    break;
            }

            return true;
        }

        /**
         * Handle purge
         *
         * @param request
         * @return std::vector<std::byte>
         */
        std::vector<std::byte> handle_purge(const request_purge &request) {
            const request_key _key{std::string(request.consumer_id_), std::string(request.resource_id_)};
            const auto _now = std::chrono::steady_clock::now();

            auto &_index = storage_.get<tag_by_key>();
            const auto _it = _index.find(_key);

            std::vector<std::byte> _response(5);
            std::memcpy(_response.data(), &request.header_->request_id_, sizeof(uint32_t));

            if (_it == _index.end() || _now >= _it->entry_.expires_at_) {
                _response[4] = std::byte{0x00};
                return _response;
            }

            _index.erase(_it);
            _response[4] = std::byte{0x01};
            return _response;
        }

        /**
         * Start garbage collector
         */
        void start_garbage_collector() {
            boost::asio::post(strand_, [self = shared_from_this()] {
                self->collect_and_flush();
            });
        }
    private:
        /**
         * Cleanup expired entries
         */
        void collect_and_flush() {
            const auto _now = std::chrono::steady_clock::now();
            const auto _threshold = std::chrono::seconds(30);

            while (!expired_entries_.empty() && _now - expired_entries_.front().second > _threshold) {
                expired_entries_.pop_front();
            }

            expiration_timer_.expires_after(std::chrono::seconds(10));
            expiration_timer_.async_wait([self = shared_from_this()](const boost::system::error_code& ec) {
                if (!ec) {
                    self->collect_and_flush();
                }
            });
        }

        /**
         * Expired entries
         */
        std::deque<std::pair<entry_wrapper, std::chrono::steady_clock::time_point>> expired_entries_;

        /**
         * Schedule expiration
         */
        void schedule_expiration(const std::chrono::steady_clock::time_point proposed) {
            auto &_schedule_index = storage_.get<tag_by_expiration>();
            if (_schedule_index.empty()) return;

            const auto &_entry = _schedule_index.begin()->entry_;

            if (proposed > _entry.expires_at_) return;

            const auto _delay = _entry.expires_at_ - std::chrono::steady_clock::now();

            expiration_timer_.expires_after(_delay);
            expiration_timer_.async_wait([_self = shared_from_this()](const boost::system::error_code &ec) {
                if (ec) return;

                auto &_timer_index = _self->storage_.get<tag_by_expiration>();

                const auto _now = std::chrono::steady_clock::now();
                while (!_timer_index.empty() && _timer_index.begin()->entry_.expires_at_ <= _now) {
                    auto _it = _timer_index.begin();
                    entry_wrapper _scoped_entry = *_it;
                    _self->expired_entries_.emplace_back(std::move(_scoped_entry), _now);
                    _timer_index.erase(_timer_index.begin());
                }

                if (!_timer_index.empty()) {
                    const auto next = _timer_index.begin()->entry_.expires_at_;
                    _self->schedule_expiration(next);
                }
            });
        }
    };
}

#endif // THROTTR_STATE_HPP
