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
#include <unordered_map>
#include <cstring>
#include <boost/core/ignore_unused.hpp>

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
        explicit state(boost::asio::io_context &ioc) : expiration_timer_(ioc), strand_(ioc.get_executor()) { }


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

            auto& _index = storage_.get<tag_by_expiration>();

            auto [_, _inserted] = storage_.insert({
                _key,
                {request.header_->quota_, request.header_->ttl_type_, _expires_at}
            });

            boost::ignore_unused(_);

            if (_inserted) {
                if (const auto& _entry = _index.begin()->entry_; _expires_at <= _entry.expires_at_) {
                    boost::asio::post(strand_, [_self = shared_from_this(), _expires_at] {
                        _self->schedule_expiration(_expires_at);
                    });
                }
            }

            return {
                static_cast<std::byte>(request.header_->request_id_),
                static_cast<std::byte>(_inserted)
            };
        }

        /**
         * Handle query
         *
         * @param request
         * @return std::vector<std::byte>
         */
        std::vector<std::byte> handle_query(const request_query &request) {
            const request_key _key{std::string(request.consumer_id_),std::string(request.resource_id_)};

            const auto _now = std::chrono::steady_clock::now();
            auto _it = requests_.find(_key);

            bool _can = false;
            uint64_t _quota_remaining = 0;
            auto _ttl_type = ttl_types::milliseconds;
            int64_t _ttl_remaining = 0;

            if (_it != requests_.end() && _now < _it->second.expires_at_) { // LCOV_EXCL_LINE note: Partially covered.
                _can = true;
                _quota_remaining = _it->second.quota_;
                _ttl_type = _it->second.ttl_type_;
                _ttl_remaining = get_ttl(_it->second.expires_at_, _ttl_type);
            }

            std::vector<std::byte> _response;
            _response.resize(1 + sizeof(uint64_t) + 1 + sizeof(int64_t));

            _response[0] = static_cast<std::byte>(_can ? 1 : 0);
            std::memcpy(_response.data() + 1, &_quota_remaining, sizeof(_quota_remaining));
            _response[1 + sizeof(uint64_t)] = static_cast<std::byte>(_ttl_type);
            std::memcpy(_response.data() + 1 + sizeof(uint64_t) + 1, &_ttl_remaining, sizeof(_ttl_remaining));

            return _response;
        }

        /**
         * Handle update
         *
         * @param request
         * @return std::vector<std::byte>
         */
        std::vector<std::byte> handle_update(const request_update &request) {
            const request_key _key{std::string(request.consumer_id_),std::string(request.resource_id_)};

            const auto _now = std::chrono::steady_clock::now();
            const auto _it = requests_.find(_key);

            if (_it == requests_.end()) { // LCOV_EXCL_LINE note: Partially covered.
                std::vector _error_response(1, std::byte{0x00});
                return _error_response;
            }

            auto &_entry = _it->second;

            using enum change_types;
            using enum ttl_types;

            switch (request.header_->attribute_) {
                case attribute_types::quota:

                    switch (request.header_->change_) {
                        case patch:
                            _entry.quota_ = request.header_->value_;
                            break;
                        case increase:
                            _entry.quota_ += request.header_->value_;
                            break;
                        case decrease:
                            if (_entry.quota_ >= request.header_->value_) { // LCOV_EXCL_LINE note: Partially covered.
                                _entry.quota_ -= request.header_->value_;
                            } else {
                                _entry.quota_ = 0;
                            }
                            break;
                    }

                    break;
                case attribute_types::ttl:

                    std::chrono::nanoseconds _duration;

                    switch (_entry.ttl_type_) {
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
                        case patch:
                            _entry.expires_at_ = _now + _duration;
                            break;
                        case increase:
                            _entry.expires_at_ += _duration;
                            break;
                        case decrease:
                            _entry.expires_at_ -= _duration;
                            break;
                    }

                    break;
            }

            std::vector _response(1, std::byte{0x01});
            return _response;
        }

        /**
         * Handle purge
         *
         * @param request
         * @return std::vector<std::byte>
         */
        std::vector<std::byte> handle_purge(const request_purge &request) {
            const request_key _key{std::string(request.consumer_id_),std::string(request.resource_id_)};

            const auto _it = requests_.find(_key);

            if (_it == requests_.end()) { // LCOV_EXCL_LINE note: Partially covered.
                std::vector _error_response(1, std::byte{0x00});
                return _error_response;
            }

            requests_.erase(_it);

            std::vector _success_response(1, std::byte{0x01});
            return _success_response;
        }


    private:

        /**
         * Schedule expiration
         */
        void schedule_expiration(const std::chrono::steady_clock::time_point proposed) {
            auto& _schedule_index = storage_.get<tag_by_expiration>();
            if (_schedule_index.empty()) return;

            const auto& _entry = _schedule_index.begin()->entry_;

            if (proposed > _entry.expires_at_) return;

            const auto _delay = _entry.expires_at_ - std::chrono::steady_clock::now();

            expiration_timer_.expires_after(_delay);
            expiration_timer_.async_wait([_self = shared_from_this()](const boost::system::error_code& ec) {
                if (ec) return;

                auto& _timer_index = _self->storage_.get<tag_by_expiration>();

                const auto _now = std::chrono::steady_clock::now();
                while (!_timer_index.empty() && _timer_index.begin()->entry_.expires_at_ <= _now) {
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
