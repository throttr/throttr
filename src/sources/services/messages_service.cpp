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

#include <throttr/protocol_wrapper.hpp>
#include <throttr/services/messages_service.hpp>

namespace throttr
{
  static std::size_t get_insert_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_insert_header_size)
      return 0;
    auto *h = reinterpret_cast<const request_insert_header *>(buffer.data());
    return request_insert_header_size + h->key_size_;
  }

  static std::size_t get_query_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_query_header_size)
      return 0;
    auto *h = reinterpret_cast<const request_query_header *>(buffer.data());
    return request_query_header_size + h->key_size_;
  }

  static std::size_t get_update_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_update_header_size)
      return 0;
    auto *h = reinterpret_cast<const request_update_header *>(buffer.data());
    return request_update_header_size + h->key_size_;
  }

  static std::size_t get_set_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_set_header_size)
      return 0;
    auto *h = reinterpret_cast<const request_set_header *>(buffer.data());
    return request_set_header_size + h->key_size_ + h->value_size_;
  }

  static std::size_t get_get_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_get_header_size)
      return 0;
    auto *h = reinterpret_cast<const request_get_header *>(buffer.data());
    return request_get_header_size + h->key_size_;
  }

  static std::size_t get_purge_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_purge_header_size)
      return 0;
    auto *h = reinterpret_cast<const request_purge_header *>(buffer.data());
    return request_purge_header_size + h->key_size_;
  }

  static std::size_t get_stat_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_stat_header_size)
      return 0;
    auto *h = reinterpret_cast<const request_stat_header *>(buffer.data());
    return request_stat_header_size + h->key_size_;
  }

  static std::size_t get_list_size(const std::span<const std::byte> &buffer)
  {
    return buffer.size() >= request_list_header_size ? request_list_header_size : 0;
  }

  static std::size_t get_stats_size(const std::span<const std::byte> &buffer)
  {
    return buffer.size() >= request_stats_header_size ? request_stats_header_size : 0;
  }

  static std::size_t get_connections_size(const std::span<const std::byte> &buffer)
  {
    return buffer.size() >= request_connections_header_size ? request_connections_header_size : 0;
  }

  static std::size_t invalid_size(const std::span<const std::byte> &)
  {
    return 0;
  }

  messages_service::messages_service()
  {
    message_types_.fill(&invalid_size);
    message_types_[static_cast<std::size_t>(request_types::insert)] = &get_insert_size;
    message_types_[static_cast<std::size_t>(request_types::set)] = &get_set_size;
    message_types_[static_cast<std::size_t>(request_types::query)] = &get_query_size;
    message_types_[static_cast<std::size_t>(request_types::get)] = &get_get_size;
    message_types_[static_cast<std::size_t>(request_types::update)] = &get_update_size;
    message_types_[static_cast<std::size_t>(request_types::purge)] = &get_purge_size;
    message_types_[static_cast<std::size_t>(request_types::list)] = &get_list_size;
    message_types_[static_cast<std::size_t>(request_types::stat)] = &get_stat_size;
    message_types_[static_cast<std::size_t>(request_types::stats)] = &get_stats_size;
    message_types_[static_cast<std::size_t>(request_types::connections)] = &get_connections_size;
  }
} // namespace throttr
