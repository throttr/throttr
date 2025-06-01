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
#include <boost/uuid/uuid_io.hpp>
#include <throttr/connection.hpp>
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
    std::vector<std::uint8_t> &write_buffer,
    const std::shared_ptr<connection> &conn)
  {
    boost::ignore_unused(conn);

    const auto _request = request_query::from_buffer(view);

    const bool _as_query = type == request_types::query;

    const request_key _key{std::string_view(reinterpret_cast<const char *>(_request.key_.data()), _request.key_.size())};
    const auto _find = state->finder_->find_or_fail_for_batch(state, _key, batch);

    if (!_find.has_value()) // LCOV_EXCL_LINE Note: Partially tested.
    {
      // LCOV_EXCL_START
#ifndef NDEBUG
      fmt::println(
        "{:%Y-%m-%d %H:%M:%S} REQUEST {} key={} id={} RESPONSE ok=false",
        std::chrono::system_clock::now(),
        _as_query ? "QUERY" : "GET",
        _key.key_,
        to_string(conn->id_));
#endif
      // LCOV_EXCL_STOP
      return;
    }
    const auto _it = _find.value();
    const value_type _ttl = get_ttl(_it->entry_.expires_at_, _it->entry_.ttl_type_);

    // status_
    batch.emplace_back(boost::asio::buffer(&state::success_response_, 1));

    if (_as_query) // LCOV_EXCL_LINE
    {
      // Value
      batch.push_back(boost::asio::buffer(_it->entry_.value_.data(), sizeof(value_type)));
      // TTL Type
      batch.push_back(boost::asio::buffer(&_it->entry_.ttl_type_, sizeof(_it->entry_.ttl_type_)));
      // TTL
      {
        const auto _offset = write_buffer.size();
        std::uint8_t ttl_bytes[sizeof(_ttl)];
        std::memcpy(ttl_bytes, &_ttl, sizeof(_ttl));
        write_buffer.insert(write_buffer.end(), ttl_bytes, ttl_bytes + sizeof(_ttl));
        batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(_ttl)));
      }
    }
    else
    {
      // TTL Type
      batch.emplace_back(boost::asio::buffer(&_it->entry_.ttl_type_, sizeof(_it->entry_.ttl_type_)));
      // TTL
      {
        const auto _offset = write_buffer.size();
        std::uint8_t ttl_bytes[sizeof(_ttl)];
        std::memcpy(ttl_bytes, &_ttl, sizeof(_ttl));
        write_buffer.insert(write_buffer.end(), ttl_bytes, ttl_bytes + sizeof(_ttl));
        batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(_ttl)));
      }
      // Size
      {
        const auto _offset = write_buffer.size();
        const auto _size = _it->entry_.value_.size();
        std::uint8_t size_bytes[sizeof(_size)];
        std::memcpy(size_bytes, &_size, sizeof(_size));
        write_buffer.insert(write_buffer.end(), size_bytes, size_bytes + sizeof(_size));
        batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(_size)));
      }

      // Value
      batch.emplace_back(boost::asio::buffer(_it->entry_.value_.data(), _it->entry_.value_.size()));
    }

    // LCOV_EXCL_START
#ifndef NDEBUG
    if (_as_query)
    {
      auto *_quota = reinterpret_cast<const value_type *>(_it->entry_.value_.data()); // NOSONAR
      fmt::println(
        "{:%Y-%m-%d %H:%M:%S} REQUEST QUERY key={} from={} RESPONSE ok=true quota={} "
        "ttl_type={} ttl={}",
        std::chrono::system_clock::now(),
        _key.key_,
        to_string(conn->id_),
        *_quota,
        to_string(_it->entry_.ttl_type_),
        _ttl);
    }
    else
    {
      fmt::println(
        "{:%Y-%m-%d %H:%M:%S} REQUEST GET key={} from={} RESPONSE ok=true value={} "
        "ttl_type={} ttl={}",
        std::chrono::system_clock::now(),
        _key.key_,
        to_string(conn->id_),
        span_to_hex(std::span(_it->entry_.value_.data(), _it->entry_.value_.size())),
        to_string(_it->entry_.ttl_type_),
        _ttl);
    }
#endif
  }
} // namespace throttr
