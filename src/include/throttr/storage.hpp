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

#include <throttr/protocol.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/tag.hpp>

namespace throttr {
    /**
     * Entry wrapper
     */
    struct entry_wrapper {
        request_key key_;
        request_entry entry_;
    };

    /**
     * Tag: access by key
     */
    struct tag_by_key { };

    /**
     * Tag: access by expiration
     */
    struct tag_by_expiration { };

    /**
     * Multi-index container type for request storage
     */
    using storage_type = boost::multi_index::multi_index_container<
        entry_wrapper,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<
                boost::multi_index::tag<tag_by_key>,
                boost::multi_index::member<entry_wrapper, request_key, &entry_wrapper::key_>,
                request_key_hasher
            >,
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<tag_by_expiration>,
                boost::multi_index::member<entry_wrapper, request_entry, &entry_wrapper::entry_>
            >
        >
    >;
}


#endif // THROTTR_STORAGE_HPP