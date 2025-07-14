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

#include <throttr/utils.hpp>

#include <throttr/entry.hpp>

#include <boost/endian/conversion.hpp>

namespace throttr
{
  entry::entry(
    const entry_types type,
    const std::span<const std::byte> value,
    const ttl_types ttl_type,
    const uint64_t expires_at) :
      type_(type), buffer_storage_(buffers_pool::take_one()), ttl_type_(ttl_type), expires_at_(expires_at)
  {
    if (type == entry_types::raw)
    {
      const auto _buffer = buffer_storage_->buffer_.load(std::memory_order_relaxed);

      _buffer->reserve(value.size());
      _buffer->resize(value.size());
      std::memcpy(_buffer->data(), value.data(), value.size());

      buffer_storage_->buffer_.store(_buffer, std::memory_order_relaxed);
    }
    else
    {
      value_type _parsed_value = 0;
      std::memcpy(&_parsed_value, value.data(), sizeof(value_type));
      counter_.store(boost::endian::little_to_native(_parsed_value), std::memory_order_release);
    }
  }

  entry::~entry()
  {
    buffer_storage_->recyclable_ = true;
  }

  void entry::update_buffer(const std::span<const std::byte> value) const
  {
    const auto _buffer = buffer_storage_->buffer_.load(std::memory_order_relaxed);

    _buffer->reserve(value.size());
    _buffer->resize(value.size());
    std::memcpy(_buffer->data(), value.data(), value.size());

    buffer_storage_->buffer_.store(_buffer, std::memory_order_relaxed);
  }
} // namespace throttr