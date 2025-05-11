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

#include <throttr/storage.hpp>
#include <throttr/response.hpp>
#include <throttr/time.hpp>
#include <throttr/utils.hpp>

#include <throttr/protocol_wrapper.hpp>

#include <memory>
#include <atomic>
#include <vector>
#include <cstring>
#include <deque>
#include <boost/core/ignore_unused.hpp>
#include <boost/asio/strand.hpp>

#include <fmt/chrono.h>

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
         * Scheduled key
         */
        std::span<const std::byte> scheduled_key_;

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
         * @param view
         * @return std::shared_ptr<response_holder>
         */
        std::shared_ptr<response_holder> handle_insert(const std::span<const std::byte> view) {
            const auto _request = request_insert::from_buffer(view);

            const std::string _key{_request.key_};

            const auto _now = std::chrono::steady_clock::now();
            const auto _expires_at = get_expiration_point(_now, _request.header_->ttl_type_, _request.header_->ttl_);

            request_entry _scoped_entry{
                entry_types::counter,
                std::vector(view.begin() + 1, view.begin() + 1 + sizeof(value_type)),
                _request.header_->ttl_type_,
                _expires_at
            };

            auto &_index = storage_.get<tag_by_key_and_valid>();

            auto [_it, _inserted] = storage_.insert(entry_wrapper{
                std::vector( // LCOV_EXCL_LINE Note: This is actually tested.
                    reinterpret_cast<const std::byte*>(_request.key_.data()), // NOSONAR
                    reinterpret_cast<const std::byte*>(_request.key_.data() + _request.key_.size()) // NOSONAR
                ),
                std::move(_scoped_entry)
            });

            boost::ignore_unused(_it);

            if (_inserted) { // LCOV_EXCL_LINE note: Partially covered.
                if (const auto &_entry = _index.begin()->entry_; _expires_at <= _entry.expires_at_) { // LCOV_EXCL_LINE note: Partially covered.
                    boost::asio::post(strand_, [_self = shared_from_this(), _expires_at] {
                        _self->schedule_expiration(_expires_at);
                    });
                }
            }

            // LCOV_EXCL_START
#ifndef NDEBUG
            fmt::println("{:%Y-%m-%d %H:%M:%S} REQUEST INSERT key={} quota={} ttl_type={} ttl={} RESPONSE ok={}", std::chrono::system_clock::now(), _key, _request.header_->quota_, to_string(_request.header_->ttl_type_), _request.header_->ttl_, _inserted);
#endif
            // LCOV_EXCL_STOP

            return std::make_shared<response_holder>(static_cast<uint8_t>(_inserted));
        }

        /**
         * Handle set
         *
         * @param view
         * @return std::shared_ptr<response_holder>
         */
        std::shared_ptr<response_holder> handle_set(const std::span<const std::byte> view) {
            const auto _request = request_set::from_buffer(view);

            const std::string _key{_request.key_};

            const auto _now = std::chrono::steady_clock::now();
            const auto _expires_at = get_expiration_point(_now, _request.header_->ttl_type_, _request.header_->ttl_);

            request_entry _scoped_entry{
                entry_types::raw,
                _request.value_,
                _request.header_->ttl_type_,
                _expires_at
            };

            auto &_index = storage_.get<tag_by_key_and_valid>();

            auto [_it, _inserted] = storage_.insert(entry_wrapper{
                std::vector( // LCOV_EXCL_LINE Note: This is actually tested.
                    reinterpret_cast<const std::byte*>(_request.key_.data()), // NOSONAR
                    reinterpret_cast<const std::byte*>(_request.key_.data() + _request.key_.size()) // NOSONAR
                ),
                std::move(_scoped_entry)
            });

            boost::ignore_unused(_it);

            if (_inserted) { // LCOV_EXCL_LINE note: Partially covered.
                if (const auto &_entry = _index.begin()->entry_; _expires_at <= _entry.expires_at_) { // LCOV_EXCL_LINE note: Partially covered.
                    boost::asio::post(strand_, [_self = shared_from_this(), _expires_at] {
                        _self->schedule_expiration(_expires_at);
                    });
                }
            }

            // LCOV_EXCL_START
#ifndef NDEBUG
            fmt::println("{:%Y-%m-%d %H:%M:%S} REQUEST SET key={} value={}ttl_type={} ttl={} RESPONSE ok={}", std::chrono::system_clock::now(), _key, span_to_hex(_request.value_), to_string(_request.header_->ttl_type_), _request.header_->ttl_, _inserted);
#endif
            // LCOV_EXCL_STOP

            return std::make_shared<response_holder>(static_cast<uint8_t>(_inserted));
        }

        /**
         * Handle query
         *
         * @param request
         * @return std::shared_ptr<response_holder>
         */
        std::shared_ptr<response_holder> handle_query(const request_query &request) {
            const request_key _key{request.key_};

            auto &_index = storage_.get<tag_by_key_and_valid>();
            const auto _it = _index.find(std::make_tuple(_key, false));

            if (_it == _index.end()) { // LCOV_EXCL_LINE note: Partially covered.

                // LCOV_EXCL_START
#ifndef NDEBUG
                fmt::println("{:%Y-%m-%d %H:%M:%S} REQUEST QUERY key={} RESPONSE ok={}", std::chrono::system_clock::now(), _key.key_, false);
#endif
                // LCOV_EXCL_STOP

                return std::make_shared<response_holder>(0x00);
            }

            const auto &_entry = _it->entry_;
            const auto _ttl = get_ttl(_entry.expires_at_, _entry.ttl_type_);

            // LCOV_EXCL_START
#ifndef NDEBUG
            auto * _quota = reinterpret_cast<const value_type *>(_entry.value_.data());
            fmt::println("{:%Y-%m-%d %H:%M:%S} REQUEST QUERY key={} RESPONSE ok={} quota={} ttl_type={} ttl={}", std::chrono::system_clock::now(), _key.key_, true, *_quota, to_string(_entry.ttl_type_), _ttl);
#endif
            // LCOV_EXCL_STOP

            return std::make_shared<response_holder>(_entry, _ttl, false);
        }

        /**
         * Handle get
         *
         * @param request
         * @return std::shared_ptr<response_holder>
         */
        std::shared_ptr<response_holder> handle_get(const request_get &request) {
            const request_key _key{request.key_};

            auto &_index = storage_.get<tag_by_key_and_valid>();
            const auto _it = _index.find(std::make_tuple(_key, false));

            if (_it == _index.end()) { // LCOV_EXCL_LINE note: Partially covered.

                // LCOV_EXCL_START
#ifndef NDEBUG
                fmt::println("{:%Y-%m-%d %H:%M:%S} REQUEST GET key={} RESPONSE ok={}", std::chrono::system_clock::now(), _key.key_, false);
#endif
                // LCOV_EXCL_STOP

                return std::make_shared<response_holder>(0x00);
            }

            const auto &_entry = _it->entry_;
            const auto _ttl = get_ttl(_entry.expires_at_, _entry.ttl_type_);

            // LCOV_EXCL_START
#ifndef NDEBUG
            fmt::println("{:%Y-%m-%d %H:%M:%S} REQUEST GET key={} RESPONSE ok={} value={}ttl_type={} ttl={}", std::chrono::system_clock::now(), _key.key_, true, span_to_hex(_entry.value_), to_string(_entry.ttl_type_), _ttl);
#endif
            // LCOV_EXCL_STOP

            return std::make_shared<response_holder>(_entry, _ttl, true);
        }

        /**
         * Handle update
         *
         * @param request
         * @return std::shared_ptr<response_holder>
         */
        std::shared_ptr<response_holder> handle_update(const request_update &request) {
            const request_key _key{request.key_};
            const auto _now = std::chrono::steady_clock::now();

            auto &_index = storage_.get<tag_by_key_and_valid>();
            const auto _it = _index.find(std::make_tuple(_key, false));

            if (_it == _index.end()) { // LCOV_EXCL_LINE note: Partially covered.
                return std::make_shared<response_holder>(0x00);
            }

            using enum attribute_types;

            bool _modified = false;

            storage_.modify(_it, [&](entry_wrapper & object) {
                switch (request.header_->attribute_) {
                    case quota:
                        _modified = apply_quota_change(object.entry_, request);
                        break;
                    case ttl:
                        _modified = apply_ttl_change(object.entry_, request, _now, object.key_);
                        break;
                }
            });

            // LCOV_EXCL_START
#ifndef NDEBUG
            fmt::println("{:%Y-%m-%d %H:%M:%S} REQUEST UPDATE key={} attribute={} change={} value={} RESPONSE ok={}", std::chrono::system_clock::now(), _key.key_, to_string(request.header_->attribute_), to_string(request.header_->change_), request.header_->value_, _modified);
#endif
            // LCOV_EXCL_STOP

            return std::make_shared<response_holder>(static_cast<uint8_t>(_modified));
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

            auto * _quota = reinterpret_cast<value_type *>(entry.value_.data());

            switch (request.header_->change_) {
                case patch:
                    *_quota = request.header_->value_;
                    break;
                case increase:
                    *_quota += request.header_->value_;
                    break;
                case decrease:
                    if (*_quota >= request.header_->value_) { // LCOV_EXCL_LINE note: Partially covered.
                        *_quota -= request.header_->value_;
                    } else {
                        return false;
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
         * @param now
         * @param key
         * @return bool
         */
        bool apply_ttl_change(request_entry &entry,
                              const request_update &request,
                              const std::chrono::steady_clock::time_point &now,
                              std::span<const std::byte> key) {
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
                    entry.expires_at_ = now + _duration;
                    break;
                case change_types::increase:
                    entry.expires_at_ += _duration;
                    break;
                case change_types::decrease:
                    entry.expires_at_ -= _duration;
                    break;
            }

            if (scheduled_key_.size() == key.size() && std::equal(scheduled_key_.begin(), scheduled_key_.end(), key.begin())) { // LCOV_EXCL_LINE Note: Partially tested
                boost::asio::post(strand_, [_self = shared_from_this(), _expires_at = entry.expires_at_] {
                               _self->schedule_expiration(_expires_at);
                });
            }

            return true;
        }

        /**
         * Handle purge
         *
         * @param request
         * @return std::shared_ptr<response_holder>
         */
        std::shared_ptr<response_holder> handle_purge(const request_purge &request) {
            const request_key _key{request.key_};

            auto &_index = storage_.get<tag_by_key_and_valid>();
            const auto _it = _index.find(std::make_tuple(_key, false));

            bool _erased = true;

            if (_it == _index.end()) {// LCOV_EXCL_LINE note: Partially covered.
                _erased = false;
            }

            // LCOV_EXCL_START
#ifndef NDEBUG
            fmt::println("{:%Y-%m-%d %H:%M:%S} REQUEST PURGE key={} RESPONSE ok={}", std::chrono::system_clock::now(), _key.key_, _erased);
#endif
            // LCOV_EXCL_STOP

            if (_erased) // LCOV_EXCL_LINE Note: Partially tested.
                _index.erase(_it);

            return std::make_shared<response_holder>(static_cast<uint8_t>(_erased));
        }

        /**
         * Expired entries
         */
        std::deque<std::pair<entry_wrapper, std::chrono::steady_clock::time_point>> expired_entries_;

// LCOV_EXCL_START
        /**
         * Expiration timer
         */
        void expiration_timer() {
            const auto _now = std::chrono::steady_clock::now();
            auto &_timer_index = storage_.get<tag_by_expiration>();

            std::vector<request_key> _to_expire;
            for (const auto &_item : _timer_index) {
                if (_item.entry_.expires_at_ > _now) break;
                _to_expire.emplace_back(_item.key());
            }

            auto &_valid_index = storage_.get<tag_by_key_and_valid>();

            for (const auto &_key : _to_expire) {
                if (auto _it = _valid_index.find(std::make_tuple(_key, false)); _it != _valid_index.end()) {
                    _valid_index.modify(_it, [](entry_wrapper &entry) {
#ifndef NDEBUG
fmt::println("{:%Y-%m-%d %H:%M:%S} SCHEDULER KEY EXPIRED key={}", std::chrono::system_clock::now(), std::string_view(reinterpret_cast<const char*>(entry.key_.data()), entry.key_.size())); // NOSONAR
#endif
                        entry.expired_ = true;
                    });
                }

                if (auto _it = _valid_index.find(std::make_tuple(_key, true)); _it != _valid_index.end()) {
                    if (auto _projected = storage_.project<tag_by_expiration>(_it); _projected != _timer_index.end()) {
#ifndef NDEBUG
                        fmt::println("{:%Y-%m-%d %H:%M:%S} SCHEDULER KEY ERASED key={}", std::chrono::system_clock::now(), std::string_view(reinterpret_cast<const char*>(_it->key_.data()), _it->key_.size())); // NOSONAR
#endif
                        _timer_index.erase(_projected);
                    }
                }
            }

            if (!_timer_index.empty()) {
                const auto _next = _timer_index.begin()->entry_.expires_at_;
                schedule_expiration(_next);
            }
        }

        /**
         * Schedule expiration
         */
        void schedule_expiration(const std::chrono::steady_clock::time_point proposed) {
#ifndef NDEBUG
            fmt::println("{:%Y-%m-%d %H:%M:%S} SCHEDULER GARBAGE COLLECTION STARTED", std::chrono::system_clock::now());
#endif

            auto &_schedule_index = storage_.get<tag_by_expiration>();
            if (_schedule_index.empty()) return;

            const auto &_item = _schedule_index.begin();
            const auto &_entry = _item->entry_;

            if (proposed > _entry.expires_at_) return;

            const auto _delay = _entry.expires_at_ - std::chrono::steady_clock::now();
            scheduled_key_ = std::span(_item->key_);

#ifndef NDEBUG
            fmt::println("{:%Y-%m-%d %H:%M:%S} SCHEDULER GARBAGE COLLECTION FINISHED", std::chrono::system_clock::now());
#endif

            expiration_timer_.expires_after(_delay);
            expiration_timer_.async_wait([_self = shared_from_this()](const boost::system::error_code &ec) {
                if (!ec) _self->expiration_timer();
            });
        }
    };
}
// LCOV_EXCL_STOP

#endif // THROTTR_STATE_HPP
