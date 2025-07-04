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

    const request_key _key{
      std::string_view(reinterpret_cast<const char *>(_request.key_.data()), _request.key_.size())}; // NOSONAR
    const auto _find = state->finder_->find_or_fail_for_batch(state, _key, batch);

    if (!_find.has_value()) // LCOV_EXCL_LINE Note: Partially tested.
    {
      // LCOV_EXCL_START
#ifndef NDEBUG
      fmt::println(
        "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST {} session_id={} META key={} RESPONSE ok=false",
        to_string(state->id_),
        std::chrono::system_clock::now(),
        _as_query ? "QUERY" : "GET",
        to_string(id),
        _key.key_);
#endif
      // LCOV_EXCL_STOP
      return;
    }
    const auto _it = _find.value();
    const value_type _ttl = get_ttl(_it->entry_.expires_at_.load(std::memory_order_relaxed), _it->entry_.ttl_type_);

    // status_
    batch.emplace_back(&state::success_response_, 1);

    if (_as_query) // LCOV_EXCL_LINE
    {
      // Value
      {
        const auto _offset = write_buffer.size();
        const value_type _counter_value = _it->entry_.counter_.load(std::memory_order_relaxed);
        append_value_type(write_buffer, native_to_little(_counter_value));
        batch.emplace_back(&write_buffer[_offset], sizeof(value_type));
      }
      // TTL Type
      batch.emplace_back(&_it->entry_.ttl_type_, sizeof(_it->entry_.ttl_type_));
      // TTL
      {
        const auto _offset = write_buffer.size();
        append_value_type(write_buffer, native_to_little(_ttl));
        batch.emplace_back(&write_buffer[_offset], sizeof(_ttl));
      }
    }
    else
    {
      // TTL Type
      batch.emplace_back(&_it->entry_.ttl_type_, sizeof(_it->entry_.ttl_type_));
      // TTL
      {
        const auto _offset = write_buffer.size();
        append_value_type(write_buffer, native_to_little(_ttl));
        batch.emplace_back(&write_buffer[_offset], sizeof(_ttl));
      }

      const auto _buffer = _it->entry_.buffer_.load(std::memory_order_acquire);

      // Size
      {
        const auto _offset = write_buffer.size();
        append_value_type(write_buffer, native_to_little(_buffer->size()));
        batch.emplace_back(&write_buffer[_offset], sizeof(value_type));
      }

      // Value
      {
        const auto _offset = write_buffer.size();
        write_buffer.resize(_offset + _buffer->size());
        std::memcpy(write_buffer.data() + _offset, _buffer->data(), _buffer->size());
        batch.emplace_back(write_buffer.data() + _offset, _buffer->size());
      }
    }

    // LCOV_EXCL_START
#ifndef NDEBUG
    if (_as_query)
    {
      auto _quota = _it->entry_.counter_.load(std::memory_order_relaxed);
      fmt::println(
        "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST QUERY session_id={} META key={} RESPONSE ok=true META quota={} ttl_type={} ttl={}",
        to_string(state->id_),
        std::chrono::system_clock::now(),
        to_string(id),
        _key.key_,
        _quota,
        to_string(_it->entry_.ttl_type_),
        _ttl);
    }
    else
    {
      const auto _buffer = _it->entry_.buffer_.load(std::memory_order_acquire);
      fmt::println(
        "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST GET session_id={} META key={} RESPONSE ok=true META value={} ttl_type={} ttl={}",
        to_string(state->id_),
        std::chrono::system_clock::now(),
        to_string(id),
        _key.key_,
        span_to_hex(std::span(_buffer->data(), _buffer->size())),
        to_string(_it->entry_.ttl_type_),
        _ttl);
    }
#endif
  }
} // namespace throttr
