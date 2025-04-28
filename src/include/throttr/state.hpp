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

#include <throttr/request.hpp>

#include <memory>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <cstring>

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
         * Requests
         */
        std::unordered_map<request_key, request_entry, request_key_hasher> requests_;

        /**
         * Calculate TTL remaining
         *
         * @param expires_at
         * @param ttl
         * @return int64_t
         */
        static int64_t calculate_ttl_remaining(
            const std::chrono::steady_clock::time_point &expires_at,
            const ttl_types ttl
        ) {
            const auto _now = std::chrono::steady_clock::now();
            if (expires_at <= _now) { // LCOV_EXCL_LINE note: Partially covered.
                return 0;
            }
            const auto _diff = expires_at - _now;
            if (ttl == ttl_types::nanoseconds) { // LCOV_EXCL_LINE note: Partially covered.
                return duration_cast<std::chrono::nanoseconds>(_diff).count();
            }

            if (ttl == ttl_types::milliseconds) {  // LCOV_EXCL_LINE note: Partially covered.
                return duration_cast<std::chrono::milliseconds>(_diff).count();
            }

            return duration_cast<std::chrono::seconds>(_diff).count();
        }

        /**
         * Calculate expiration point
         *
         * @param now
         * @param ttl_type
         * @param ttl
         * @return std::chrono::steady_clock::time_point
         */
        static std::chrono::steady_clock::time_point calculate_expiration_point(
            const std::chrono::steady_clock::time_point &now,
            const ttl_types ttl_type,
            const uint64_t ttl
        ) {
            if (ttl_type == ttl_types::nanoseconds) {
                return now + std::chrono::nanoseconds(ttl);
            }

            if (ttl_type == ttl_types::milliseconds) { // LCOV_EXCL_LINE note: Partially covered.
                return now + std::chrono::milliseconds(ttl);
            }

            return now + std::chrono::seconds(ttl);
        }


        /**
         * Handle insert
         *
         * @param request
         * @return std::vector<std::byte>
         */
        std::vector<std::byte> handle_insert(const request_insert &request) {
            const request_key _key{
                std::string(request.consumer_id_),
                std::string(request.resource_id_)
            };

            const auto _now = std::chrono::steady_clock::now();
            auto _it = requests_.find(_key);

            if (_it != requests_.end() && _now >= _it->second.expires_at_) {  // LCOV_EXCL_LINE note: Partially covered.
                requests_.erase(_it);
                _it = requests_.end();
            }

            bool _can = false;
            uint64_t _quota_remaining = 0;
            ttl_types _ttl_type = request.header_->ttl_type_;
            int64_t _ttl_remaining = 0;

            if (_it != requests_.end()) {  // LCOV_EXCL_LINE note: Partially covered.
                auto &_entry = _it->second;
                if (_entry.quota_ >= request.header_->usage_) {  // LCOV_EXCL_LINE note: Partially covered.
                    _entry.quota_ -= request.header_->usage_;
                    _can = true;
                }
                _quota_remaining = _entry.quota_;
                _ttl_remaining = calculate_ttl_remaining(_entry.expires_at_, _ttl_type);
            } else {
                const auto _expires_at = calculate_expiration_point(_now, _ttl_type, request.header_->ttl_);
                requests_[_key] = request_entry{
                    request.header_->quota_ - request.header_->usage_,
                    _ttl_type,
                    _expires_at
                };
                _can = true;
                _quota_remaining = request.header_->quota_ - request.header_->usage_;
                _ttl_remaining = calculate_ttl_remaining(_expires_at, _ttl_type);
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
         * Handle query
         *
         * @param request
         * @return std::vector<std::byte>
         */
        std::vector<std::byte> handle_query(const request_query &request) {
            const request_key _key{
                std::string(request.consumer_id_),
                std::string(request.resource_id_)
            };

            const auto _now = std::chrono::steady_clock::now();
            auto _it = requests_.find(_key);

            bool _can = false;
            uint64_t _quota_remaining = 0;
            auto _ttl_type = ttl_types::milliseconds;
            int64_t _ttl_remaining = 0;

            if (_it != requests_.end() && _now < _it->second.expires_at_) {  // LCOV_EXCL_LINE note: Partially covered.
                _can = true;
                _quota_remaining = _it->second.quota_;
                _ttl_type = _it->second.ttl_type_;
                _ttl_remaining = calculate_ttl_remaining(_it->second.expires_at_, _ttl_type);
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
            const request_key _key{
                std::string(request.consumer_id_),
                std::string(request.resource_id_)
            };

            const auto _now = std::chrono::steady_clock::now();
            const auto _it = requests_.find(_key);

            if (_it == requests_.end()) {
                std::vector _error_response(1, std::byte{0x00});
                return _error_response;
            }

            auto &_entry = _it->second;

            if (request.header_->attribute_ == attribute_types::quota) {
                if (request.header_->change_ == change_types::patch) {
                    _entry.quota_ = request.header_->value_;
                } else if (request.header_->change_ == change_types::increase) {
                    _entry.quota_ += request.header_->value_;
                } else if (request.header_->change_ == change_types::decrease) {
                    if (_entry.quota_ >= request.header_->value_) {
                        _entry.quota_ -= request.header_->value_;
                    } else {
                        _entry.quota_ = 0;
                    }
                }
            } else if (request.header_->attribute_ == attribute_types::ttl) {
                std::chrono::nanoseconds _duration;

                switch (_entry.ttl_type_) {
                    case ttl_types::seconds:
                        _duration = std::chrono::seconds(request.header_->value_);
                        break;
                    case ttl_types::milliseconds:
                        _duration = std::chrono::milliseconds(request.header_->value_);
                        break;
                    case ttl_types::nanoseconds:
                    default:
                        _duration = std::chrono::nanoseconds(request.header_->value_);
                        break;
                }


                switch (request.header_->change_) {
                    case change_types::patch:
                    _entry.expires_at_ = _now + _duration;
                        break;
                    case change_types::increase:
                    _entry.expires_at_ += _duration;
                        break;
                    case change_types::decrease:
                    _entry.expires_at_ -= _duration;
                        break;
                }
            }

            std::vector _response(1, std::byte{0x01});
            return _response;
        }
    };
}

#endif // THROTTR_STATE_HPP
