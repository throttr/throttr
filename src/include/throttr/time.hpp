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

#ifndef THROTTR_TIME_HPP
#define THROTTR_TIME_HPP

#include <boost/endian/conversion.hpp>
#include <chrono>
#include <throttr/protocol_wrapper.hpp>

namespace throttr
{
  /**
   * Get expiration point
   *
   * @param now
   * @param ttl_type
   * @param ttl
   * @return uint64_t
   */
  inline uint64_t get_expiration_point(const uint64_t now, const ttl_types ttl_type, const std::span<const std::byte> ttl)
  {
    using namespace std::chrono;

    value_type _value = 0;
    std::memcpy(&_value, ttl.data(), sizeof(value_type));
    _value = boost::endian::little_to_native(_value);

    std::uint64_t offset_ns = 0;

    switch (ttl_type)
    {
      case ttl_types::nanoseconds:
        offset_ns = _value;
        break;
      case ttl_types::milliseconds:
        offset_ns = duration_cast<nanoseconds>(microseconds(_value)).count();
        break;
      case ttl_types::microseconds:
        offset_ns = duration_cast<nanoseconds>(milliseconds(_value)).count();
        break;
      case ttl_types::minutes:
        offset_ns = duration_cast<nanoseconds>(minutes(_value)).count();
        break;
      case ttl_types::hours:
        offset_ns = duration_cast<nanoseconds>(hours(_value)).count();
        break;
      default:
        offset_ns = duration_cast<nanoseconds>(seconds(_value)).count();
        break;
    }
    return now + offset_ns;
  }

  /**
   * Get TTL
   *
   * @param expires_at
   * @param ttl_type
   * @return value_type
   */
  inline value_type get_ttl(const uint64_t expires_at, const ttl_types ttl_type)
  {
    using namespace std::chrono;

    const std::uint64_t now_ns = system_clock::now().time_since_epoch().count();
    if (expires_at <= now_ns)
    {
      return 0;
    }

    const std::uint64_t diff_ns = expires_at - now_ns;

    switch (ttl_type)
    {
      case ttl_types::nanoseconds:
        return diff_ns;
      case ttl_types::microseconds:
        return duration_cast<microseconds>(nanoseconds(diff_ns)).count();
      case ttl_types::milliseconds:
        return duration_cast<milliseconds>(nanoseconds(diff_ns)).count();
      case ttl_types::minutes:
        return duration_cast<minutes>(nanoseconds(diff_ns)).count();
      case ttl_types::hours:
        return duration_cast<hours>(nanoseconds(diff_ns)).count();
      default:
        return duration_cast<seconds>(nanoseconds(diff_ns)).count();
    }
  }
} // namespace throttr

#endif // THROTTR_TIME_HPP