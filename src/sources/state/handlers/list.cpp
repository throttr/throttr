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

#include <throttr/state.hpp>

namespace throttr
{
  void state::handle_list(std::vector<boost::asio::const_buffer> &batch, std::vector<std::uint8_t> &write_buffer)
  {
    auto _self = shared_from_this();
    handle_fragmented_response(
      batch,
      write_buffer,
      2048,
      [_write_buffer_ref = // LCOV_EXCL_LINE Note: For some reason this line isn't tested ...
       std::ref(write_buffer)](std::vector<boost::asio::const_buffer> *b, const entry_wrapper *e, const bool measure)
      { return write_list_entry_to_buffer(b, e, _write_buffer_ref, measure); });
  }

  std::size_t state::write_list_entry_to_buffer(
    std::vector<boost::asio::const_buffer> *batch,
    const entry_wrapper *entry,
    std::vector<std::uint8_t> &write_buffer,
    const bool measure)
  {
    if (measure) // LCOV_EXCL_LINE Note: Partially tested.
      return entry->key_.size() + entry->entry_.value_.size() + 11;

    const auto _offset = write_buffer.size();
    write_buffer.push_back(static_cast<std::uint8_t>(entry->key_.size()));
    batch->emplace_back(boost::asio::buffer(&write_buffer[_offset], 1));
    batch->emplace_back(boost::asio::buffer(&entry->entry_.type_, sizeof(entry->entry_.type_)));
    batch->emplace_back(boost::asio::buffer(&entry->entry_.ttl_type_, sizeof(entry->entry_.ttl_type_)));

    {
      const auto _expires_at =
        std::chrono::duration_cast<std::chrono::nanoseconds>(entry->entry_.expires_at_.time_since_epoch()).count();
      const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&_expires_at); // NOSONAR
      const auto _off = write_buffer.size();
      write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(_expires_at));
      batch->emplace_back(boost::asio::buffer(&write_buffer[_off], sizeof(_expires_at)));
    }

    {
      const auto _bytes_used = static_cast<value_type>(entry->entry_.value_.size());
      const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&_bytes_used); // NOSONAR
      const auto _off = write_buffer.size();
      write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(_bytes_used));
      batch->emplace_back(boost::asio::buffer(&write_buffer[_off], sizeof(_bytes_used)));
    }

    return 0;
  }
} // namespace throttr