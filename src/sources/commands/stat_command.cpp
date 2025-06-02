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
#include <boost/uuid/uuid_io.hpp>
#include <throttr/connection.hpp>
#include <throttr/state.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  void stat_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::uint8_t> &write_buffer,
    const std::shared_ptr<connection> &conn)
  {

    boost::ignore_unused(type, conn);

    const auto _request = request_stat::from_buffer(view);
#ifndef ENABLED_FEATURE_METRICS
    batch.emplace_back(boost::asio::buffer(&failed_response_, 1));
    return;
#endif

    const request_key _key{
      std::string_view(reinterpret_cast<const char *>(_request.key_.data()), _request.key_.size())}; // NOSONAR
    const auto _find = state->finder_->find_or_fail_for_batch(state, _key, batch);
    if (!_find.has_value()) // LCOV_EXCL_LINE
    {
      // LCOV_EXCL_START
#ifndef NDEBUG
      fmt::println(
        "{:%Y-%m-%d %H:%M:%S} REQUEST STAT key={} from={} RESPONSE ok=false",
        std::chrono::system_clock::now(),
        _key.key_,
        to_string(conn->id_));
#endif
      batch.emplace_back(boost::asio::buffer(&state::failed_response_, 1));
      return;
      // LCOV_EXCL_STOP
    }
    const auto _it = _find.value();
    batch.emplace_back(boost::asio::buffer(&state::success_response_, 1));

    auto _append_uint64 = [&write_buffer, &batch](const uint64_t value)
    {
      const auto _offset = write_buffer.size();
      std::uint8_t value_bytes[sizeof(uint64_t)];         // NOSONAR
      std::memcpy(value_bytes, &value, sizeof(uint64_t)); // Copiar los bytes de 'value' al buffer
      write_buffer.insert(write_buffer.end(), value_bytes, value_bytes + sizeof(uint64_t));
      batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(uint64_t)));
    };

    const auto &metrics = *_it->metrics_;
    _append_uint64(metrics.reads_per_minute_.load(std::memory_order_relaxed));
    _append_uint64(metrics.writes_per_minute_.load(std::memory_order_relaxed));
    _append_uint64(metrics.reads_accumulator_.load(std::memory_order_relaxed));
    _append_uint64(metrics.writes_accumulator_.load(std::memory_order_relaxed));

    // LCOV_EXCL_START
#ifndef NDEBUG
    fmt::println(
      "{:%Y-%m-%d %H:%M:%S} REQUEST STAT key={} from={} RESPONSE ok=true read_per_minute={} write_per_minute={} read_total={} "
      "write_total={}",
      std::chrono::system_clock::now(),
      _key.key_,
      to_string(conn->id_),
      metrics.reads_per_minute_.load(),
      metrics.writes_per_minute_.load(),
      metrics.reads_accumulator_.load(),
      metrics.writes_accumulator_.load());
#endif
    // LCOV_EXCL_STOP
  }
} // namespace throttr
