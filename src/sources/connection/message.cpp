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

#include <throttr/connection.hpp>
#include <throttr/protocol_wrapper.hpp>

namespace throttr
{
  std::size_t
  connection::get_message_sized(const std::span<const std::byte> buffer, const std::size_t header_size, const std::size_t extra)
  {
    // LCOV_EXCL_START
    if (buffer.size() < header_size + extra)
      return 0;
    // LCOV_EXCL_STOP
    return header_size + extra;
  }

  std::size_t connection::get_message_size(const std::span<const std::byte> buffer)
  {
    if (buffer.empty())
      return 0;

    const auto *_buffer = buffer.data();

    switch (const auto _type = static_cast<request_types>(std::to_integer<uint8_t>(_buffer[0])); _type)
    {
      case request_types::insert:
      {
        auto *_h = reinterpret_cast<const request_insert_header *>(_buffer); // NOSONAR
        return get_message_sized(buffer, request_insert_header_size, _h->key_size_);
      }
      case request_types::query:
      {
        auto *_h = reinterpret_cast<const request_query_header *>(_buffer); // NOSONAR
        return get_message_sized(buffer, request_query_header_size, _h->key_size_);
      }
      case request_types::update:
      {
        auto *_h = reinterpret_cast<const request_update_header *>(_buffer); // NOSONAR
        return get_message_sized(buffer, request_update_header_size, _h->key_size_);
      }
      case request_types::purge:
      {
        auto *_h = reinterpret_cast<const request_purge_header *>(_buffer); // NOSONAR
        return get_message_sized(buffer, request_purge_header_size, _h->key_size_);
      }
      case request_types::set:
      {
        auto *_h = reinterpret_cast<const request_set_header *>(_buffer); // NOSONAR
        return get_message_sized(buffer, request_set_header_size, _h->key_size_ + _h->value_size_);
      }
      case request_types::get:
      {
        auto *_h = reinterpret_cast<const request_get_header *>(_buffer); // NOSONAR
        return get_message_sized(buffer, request_get_header_size, _h->key_size_);
      }
      case request_types::list:
      {
        return get_message_sized(buffer, request_list_header_size, 0);
      }
      case request_types::stat:
      {
        auto *_h = reinterpret_cast<const request_stat_header *>(_buffer); // NOSONAR
        return get_message_sized(buffer, request_stat_header_size, _h->key_size_);
      }
      case request_types::stats:
      {
        return get_message_sized(buffer, request_stats_header_size, 0);
      }
      case request_types::connections:
      {
        return get_message_sized(buffer, request_connections_header_size, 0);
      }
        // LCOV_EXCL_START
      default:
        return 0;
        // LCOV_EXCL_STOP
    }
  }
} // namespace throttr