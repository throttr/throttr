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

#include <throttr/commands/stat_command.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <throttr/connection.hpp>
#include <throttr/services/find_service.hpp>
#include <throttr/state.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  void stat_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer,
    const boost::uuids::uuid id)
  {

    boost::ignore_unused(type, id);
    using namespace boost::endian;

    const auto _request = request_stat::from_buffer(view);

    const request_key _key{
      std::string_view(reinterpret_cast<const char *>(_request.key_.data()), _request.key_.size())}; // NOSONAR
    const auto _find = state->finder_->find_or_fail(state, _key);
    if (!_find.has_value()) // LCOV_EXCL_LINE
    {
      batch.reserve(batch.size() + 1);
      batch.emplace_back(boost::asio::const_buffer(&state::failed_response_, 1));

      // LCOV_EXCL_START
#ifndef NDEBUG
      fmt::println(
        "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST STAT session_id={} META key={} RESPONSE ok=false",
        to_string(state->id_),
        std::chrono::system_clock::now(),
        to_string(id),
        _key.key_);
#endif
      return;
      // LCOV_EXCL_STOP
    }

    const auto _it = _find.value();
    boost::ignore_unused(_it);

    write_buffer.resize(write_buffer.size() + sizeof(uint64_t) * 4); // 4 uint64_t metrics
    batch.reserve(batch.size() + 5);                                 // status + 4 metrics

    batch.emplace_back(&state::success_response_, 1);

    std::size_t _offset = 0;

#ifdef ENABLED_FEATURE_METRICS
    const auto &metrics = *_it->metrics_;
    const auto _reads_per_minute = metrics.reads_per_minute_.load(std::memory_order_relaxed);
    const auto _writes_per_minute = metrics.writes_per_minute_.load(std::memory_order_relaxed);
    const auto _reads_accumulator = metrics.reads_accumulator_.load(std::memory_order_relaxed);
    const auto _writes_accumulator = metrics.writes_accumulator_.load(std::memory_order_relaxed);

    std::memcpy(write_buffer.data() + _offset, &_reads_per_minute, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    std::memcpy(write_buffer.data() + _offset, &_writes_per_minute, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    std::memcpy(write_buffer.data() + _offset, &_reads_accumulator, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    std::memcpy(write_buffer.data() + _offset, &_writes_accumulator, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));

#else
    constexpr uint64_t _empty = 0;
    for (int _i = 0; _i < 4; ++_i)
    {
      std::memcpy(write_buffer.data() + _offset, &_empty, sizeof(uint64_t));
      batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
      _offset += sizeof(uint64_t);
    }
#endif

    // LCOV_EXCL_START
#ifndef NDEBUG
    fmt::println(
      "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST STAT session_id={} META key={} RESPONSE ok=true META read_per_minute={} "
      "write_per_minute={} "
      "read_total={} write_total={}",
      to_string(state->id_),
      std::chrono::system_clock::now(),
      to_string(id),
      _key.key_,
#ifdef ENABLED_FEATURE_METRICS
      metrics.reads_per_minute_.load(),
      metrics.writes_per_minute_.load(),
      metrics.reads_accumulator_.load(),
      metrics.writes_accumulator_.load());
#else
      0,
      0,
      0,
      0);
#endif
#endif
    // LCOV_EXCL_STOP
  }
} // namespace throttr
