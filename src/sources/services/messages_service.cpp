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
    const auto _key_size = std::to_integer<uint8_t>(buffer[sizeof(value_type) * 2 + 2]);
    return request_insert_header_size + _key_size;
  }

  static std::size_t get_query_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_query_header_size)
      return 0;
    const auto _key_size = std::to_integer<uint8_t>(buffer[1]);
    return request_query_header_size + _key_size;
  }

  static std::size_t get_update_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_update_header_size)
      return 0;
    const auto _key_size = std::to_integer<uint8_t>(buffer[sizeof(value_type) + 3]);
    return request_update_header_size + _key_size;
  }

  static std::size_t get_set_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_set_header_size)
      return 0;
    const auto _key_size = std::to_integer<uint8_t>(buffer[2 + sizeof(value_type)]);
    value_type _value_size = 0;
    for (std::size_t i = 0; i < sizeof(value_type); ++i)
    {
      _value_size |= static_cast<value_type>(std::to_integer<uint8_t>(buffer[3 + sizeof(value_type) + i])) << (8 * i);
    }
    return request_set_header_size + _key_size + _value_size;
  }

  static std::size_t get_get_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_get_header_size)
      return 0;
    const auto _key_size = std::to_integer<uint8_t>(buffer[1]);
    return request_get_header_size + _key_size;
  }

  static std::size_t get_purge_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_purge_header_size)
      return 0;
    const auto _key_size = std::to_integer<uint8_t>(buffer[1]);
    return request_purge_header_size + _key_size;
  }

  static std::size_t get_stat_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_stat_header_size)
      return 0;
    const auto _key_size = std::to_integer<uint8_t>(buffer[1]);
    return request_stat_header_size + _key_size;
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

  static std::size_t get_connection_size(const std::span<const std::byte> &buffer)
  {
    return buffer.size() >= request_connection_header_size ? request_connection_header_size : 0;
  }

  static std::size_t get_whoami_size(const std::span<const std::byte> &buffer)
  {
    return buffer.size() >= request_whoami_header_size ? request_whoami_header_size : 0;
  }

  static std::size_t get_subscribe_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_subscribe_header_size)
      return 0;
    const auto _channel_size = std::to_integer<uint8_t>(buffer[1]);
    return request_subscribe_header_size + _channel_size;
  }

  static std::size_t get_unsubscribe_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_unsubscribe_header_size)
      return 0;
    const auto _channel_size = std::to_integer<uint8_t>(buffer[1]);
    return request_unsubscribe_header_size + _channel_size;
  }

  static std::size_t get_publish_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_publish_header_size)
      return 0;
    const auto _channel_size = std::to_integer<uint8_t>(buffer[1]);
    value_type _value_size = 0;
    for (std::size_t i = 0; i < sizeof(value_type); ++i)
    {
      _value_size |= static_cast<value_type>(std::to_integer<uint8_t>(buffer[2 + i])) << (8 * i);
    }
    return request_publish_header_size + _channel_size + _value_size;
  }

  static std::size_t get_channels_size(const std::span<const std::byte> &buffer)
  {
    return buffer.size() >= request_channels_header_size ? request_channels_header_size : 0;
  }

  static std::size_t get_channel_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_channel_header_size)
      return 0;
    const auto _channel_size = std::to_integer<uint8_t>(buffer[1]);
    return request_channel_header_size + _channel_size;
  }

  static std::size_t get_info_size(const std::span<const std::byte> &buffer)
  {
    return buffer.size() >= request_whoami_header_size ? request_whoami_header_size : 0;
  }

  static std::size_t get_event_size(const std::span<const std::byte> &buffer)
  {
    if (buffer.size() < request_event_header_size)
      return 0;
    const auto _channel_size = static_cast<uint8_t>(buffer[1]);
    value_type _value_size = 0;
    for (std::size_t i = 0; i < sizeof(value_type); ++i)
    {
      _value_size |= static_cast<value_type>(std::to_integer<uint8_t>(buffer[2 + i])) << (8 * i);
    }
    return request_event_header_size + _channel_size + _value_size;
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
    message_types_[static_cast<std::size_t>(request_types::connection)] = &get_connection_size;
    message_types_[static_cast<std::size_t>(request_types::whoami)] = &get_whoami_size;
    message_types_[static_cast<std::size_t>(request_types::subscribe)] = &get_subscribe_size;
    message_types_[static_cast<std::size_t>(request_types::unsubscribe)] = &get_unsubscribe_size;
    message_types_[static_cast<std::size_t>(request_types::publish)] = &get_publish_size;
    message_types_[static_cast<std::size_t>(request_types::channels)] = &get_channels_size;
    message_types_[static_cast<std::size_t>(request_types::channel)] = &get_channel_size;
    message_types_[static_cast<std::size_t>(request_types::info)] = &get_info_size;
    message_types_[static_cast<std::size_t>(request_types::event)] = &get_event_size;
  }
} // namespace throttr
