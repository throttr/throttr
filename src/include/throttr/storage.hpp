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

#ifndef THROTTR_STORAGE_HPP
#define THROTTR_STORAGE_HPP

#include <throttr/protocol_wrapper.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/tag.hpp>

namespace throttr {
    /**
     * Entry wrapper
     */
    struct entry_wrapper {
        /**
         * Key
         */
        std::vector<std::byte> key_;

        /**
         * Entry
         */
        request_entry entry_;

        /**
         * Expired
         */
        bool expired_ = false;

        /**
         * Key
         *
         * @return
         */
        [[nodiscard]] request_key key() const {
            return {
                std::string_view(reinterpret_cast<const char *>(key_.data()), key_.size()), // NOSONAR
            };
        }

        /**
         * Entry wrapper
         *
         * @param k
         * @param e
         */
        entry_wrapper(std::vector<std::byte> k, const request_entry e) : key_(std::move(k)), entry_(e) {
        }
    };

    /**
     * Tag: access by expiration
     */
    struct tag_by_expiration {
    };

    /**
     * Tag: access by valid key
     */
    struct tag_by_key_and_valid {
    };

    /**
     * Request entry by expiration
     */
    struct request_entry_by_expiration {
        bool operator()(const request_entry &a, const request_entry &b) const {
            return a.expires_at_ < b.expires_at_;
        }
    };

    /**
     * Multi-index container type for request storage
     */
    using storage_type = boost::multi_index::multi_index_container<
        entry_wrapper,
        boost::multi_index::indexed_by<
            // Find by key and valid
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<tag_by_key_and_valid>,
                boost::multi_index::composite_key<
                    entry_wrapper,
                    boost::multi_index::const_mem_fun<entry_wrapper, request_key, &entry_wrapper::key>,
                    boost::multi_index::member<entry_wrapper, bool, &entry_wrapper::expired_>
                >,
                boost::multi_index::composite_key_hash<
                    request_key_hasher,
                    std::hash<bool>
                >,
                boost::multi_index::composite_key_equal_to<
                    std::equal_to<request_key>,
                    std::equal_to<bool>
                >
            >,
            // Find by key
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<tag_by_expiration>,
                boost::multi_index::member<entry_wrapper, request_entry, &entry_wrapper::entry_>,
                request_entry_by_expiration
            >
        >
    >;
}


#endif // THROTTR_STORAGE_HPP
