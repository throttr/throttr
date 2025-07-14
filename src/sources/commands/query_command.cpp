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

#include <throttr/commands/query_command.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <throttr/connection.hpp>
#include <throttr/services/find_service.hpp>
#include <throttr/state.hpp>
#include <throttr/time.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  void query_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer,
    const boost::uuids::uuid id)
  {
    boost::ignore_unused(id);

    using namespace boost::endian;

    const auto _request = request_query::from_buffer(view);

    const bool _as_query = type == request_types::query;

    std::string _key(_request.key_.size(), '\0');
    std::memcpy(_key.data(), _request.key_.data(), _request.key_.size());

    const request_key _request_key{_key};

    const auto _find = state->finder_->find_or_fail(state, _request_key);

    if (!_find.has_value())
    {
      batch.reserve(batch.size() + 1);
      batch.emplace_back(boost::asio::const_buffer(&state::failed_response_, 1));

#ifndef NDEBUG
      fmt::println(
        "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST {} session_id={} META key={} RESPONSE ok=false",
        to_string(state->id_),
        std::chrono::system_clock::now(),
        _as_query ? "QUERY" : "GET",
        to_string(id),
        _request_key.key_);
#endif

      return;
    }
    const auto _it = _find.value();

    const value_type _ttl = get_ttl(_it->entry_.expires_at_.load(std::memory_order_relaxed), _it->entry_.ttl_type_);

    std::size_t _offset = write_buffer.size();

    if (_as_query)
    {
      write_buffer.resize(write_buffer.size() + sizeof(value_type) * 2);
      batch.reserve(batch.size() + 4);

      // status + value + ttl + ttl_type
      batch.emplace_back(&state::success_response_, 1);

      // Value
      {
        const value_type _counter_value = _it->entry_.counter_.load(std::memory_order_relaxed);
        std::memcpy(write_buffer.data() + _offset, &_counter_value, sizeof(value_type));
        batch.emplace_back(write_buffer.data() + _offset, sizeof(value_type));
        _offset += sizeof(value_type);
      }
      // TTL Type
      batch.emplace_back(&_it->entry_.ttl_type_, sizeof(_it->entry_.ttl_type_));
      // TTL
      {
        std::memcpy(write_buffer.data() + _offset, &_ttl, sizeof(value_type));
        batch.emplace_back(write_buffer.data() + _offset, sizeof(_ttl));
      }
    }
    else
    {

      batch.reserve(batch.size() + 5);
      const auto _buffer = _it->entry_.buffer_storage_->buffer_.load(std::memory_order_acquire);
      write_buffer.resize(write_buffer.size() + sizeof(value_type) * 2 + _buffer->size());

      batch.emplace_back(&state::success_response_, 1);

      // TTL Type
      batch.emplace_back(&_it->entry_.ttl_type_, sizeof(_it->entry_.ttl_type_));

      // TTL
      {
        std::memcpy(write_buffer.data() + _offset, &_ttl, sizeof(value_type));
        batch.emplace_back(write_buffer.data() + _offset, sizeof(_ttl));
        _offset += sizeof(value_type);
      }

      // Size
      {
        const auto _buffer_size = native_to_little(_buffer->size());
        std::memcpy(write_buffer.data() + _offset, &_buffer_size, sizeof(value_type));
        batch.emplace_back(write_buffer.data() + _offset, sizeof(value_type));
        _offset += sizeof(value_type);
      }

      // Value
      {
        std::memcpy(write_buffer.data() + _offset, _buffer->data(), _buffer->size());
        batch.emplace_back(write_buffer.data() + _offset, _buffer->size());
      }
    }

#ifndef NDEBUG
    if (_as_query)
    {
      auto _quota = _it->entry_.counter_.load(std::memory_order_relaxed);
      fmt::println(
        "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST QUERY session_id={} META key={} RESPONSE ok=true META quota={} ttl_type={} ttl={}",
        to_string(state->id_),
        std::chrono::system_clock::now(),
        to_string(id),
        _request_key.key_,
        _quota,
        to_string(_it->entry_.ttl_type_),
        _ttl);
    }
    else
    {
      const auto _buffer = _it->entry_.buffer_storage_->buffer_.load(std::memory_order_acquire);
      fmt::println(
        "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST GET session_id={} META key={} RESPONSE ok=true META value={} ttl_type={} ttl={}",
        to_string(state->id_),
        std::chrono::system_clock::now(),
        to_string(id),
        _request_key.key_,
        span_to_hex(std::span(_buffer->data(), _buffer->size())),
        to_string(_it->entry_.ttl_type_),
        _ttl);
    }
#endif
  }
} // namespace throttr
