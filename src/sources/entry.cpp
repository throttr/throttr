#include <throttr/utils.hpp>

#include <iostream>
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
    buffer_storage_->can_be_reused_ = true;
  }

  void entry::update_buffer(const std::span<const std::byte> value) const
  {
    auto _buffer = buffer_storage_->buffer_.load(std::memory_order_relaxed);

    _buffer->reserve(value.size());
    _buffer->resize(value.size());
    std::memcpy(_buffer->data(), value.data(), value.size());

    buffer_storage_->buffer_.store(_buffer, std::memory_order_relaxed);
  }
} // namespace throttr