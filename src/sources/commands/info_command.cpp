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

#include <set>
#include <throttr/commands/info_command.hpp>
#include <throttr/connection.hpp>
#include <throttr/connection_metrics.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <throttr/services/metrics_collector_service.hpp>
#include <throttr/services/subscriptions_service.hpp>
#include <throttr/state.hpp>
#include <throttr/utils.hpp>
#include <throttr/version.hpp>

namespace throttr
{
  void info_command::call(
    const std::shared_ptr<state> &state,
    const request_types type,
    const std::span<const std::byte> view,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer,
    const boost::uuids::uuid id)
  {
    boost::ignore_unused(type, view, id);
    using namespace boost::endian;

    std::scoped_lock _lock(state->subscriptions_->mutex_);

    std::size_t _offset = write_buffer.size();

    // 8 bytes now
    // 16 bytes total requests + total requests per minute
    // 288 bytes on 2 (8 bytes) per type metrics (18 total)
    // 32 bytes on metrics (reads, writes) (total and per minute)
    // 40 bytes on metrics (keys, buffers, counters, allocated bytes on buffers, allocated bytes on counters)
    // 24 bytes on channels, subscriptions and started at
    // 24 bytes on total connections + version
    // ===
    // 432 bytes
    write_buffer.resize(write_buffer.size() + 432);

    // 1 status
    // 1 now
    // 2 total and per minute requests
    // 36 in per type, total and per minute requests
    // 4 in reads and writes, per minute and total
    // 5 in keys, counters, buffers, allocated on buffers and counters
    // 3 in subscriptions, channels and started_at
    // 2 in connections and version
    // ===
    // 54 segments
    batch.reserve(batch.size() + 54);

    batch.emplace_back(&state::success_response_, 1);

    const auto _now =
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    boost::ignore_unused(_now);

    uint64_t _total_requests = 0;
    uint64_t _total_requests_per_minute = 0;

    const auto _now_little = native_to_little(_now);

    std::memcpy(write_buffer.data() + _offset, &_now_little, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t); // 8 bytes

#ifdef ENABLED_FEATURE_METRICS
    const auto &metrics = state->metrics_collector_->commands_;
    for (const auto &m : metrics)
    {
      _total_requests += m.accumulator_.load(std::memory_order_relaxed);
      _total_requests_per_minute += m.per_minute_.load(std::memory_order_relaxed);
    }
#endif

    const auto _total_requests_little = native_to_little(_total_requests);
    const auto _total_requests_per_minute_little = native_to_little(_total_requests_per_minute);

    std::memcpy(write_buffer.data() + _offset, &_total_requests_little, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    std::memcpy(write_buffer.data() + _offset, &_total_requests_per_minute_little, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    for (auto _metric_type : {
           request_types::insert,
           request_types::query,
           request_types::update,
           request_types::purge,
           request_types::get,
           request_types::set,
           request_types::list,
           request_types::info,
           request_types::stats,
           request_types::stat,
           request_types::subscribe,
           request_types::unsubscribe,
           request_types::publish,
           request_types::channel,
           request_types::channels,
           request_types::whoami,
           request_types::connection,
           request_types::connections,
         }) // 18 x 8 * 2 = 288 bytes on metrics
    {
#ifdef ENABLED_FEATURE_METRICS

      const auto _accumulator =
        native_to_little(metrics[static_cast<std::size_t>(_metric_type)].accumulator_.load(std::memory_order_relaxed));
      const auto _per_minute =
        native_to_little(metrics[static_cast<std::size_t>(_metric_type)].per_minute_.load(std::memory_order_relaxed));

      std::memcpy(write_buffer.data() + _offset, &_accumulator, sizeof(uint64_t));
      batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
      _offset += sizeof(uint64_t);

      std::memcpy(write_buffer.data() + _offset, &_per_minute, sizeof(uint64_t));
      batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
      _offset += sizeof(uint64_t);

#else
      boost::ignore_unused(_metric_type);

      std::memcpy(write_buffer.data() + _offset, &_total_requests, sizeof(uint64_t));
      batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
      _offset += sizeof(uint64_t);

      std::memcpy(write_buffer.data() + _offset, &_total_requests, sizeof(uint64_t));
      batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
      _offset += sizeof(uint64_t);
#endif
    }

    uint64_t _total_read = 0;
    uint64_t _total_write = 0;
    uint64_t _total_read_per_minute = 0;
    uint64_t _total_write_per_minute = 0;
#ifdef ENABLED_FEATURE_METRICS
    auto _accumulate_metrics = [](
                                 const auto &connections,
                                 auto &mutex,
                                 auto &total_read,
                                 auto &total_write,
                                 auto &total_read_per_minute,
                                 auto &total_write_per_minute)
    {
      std::scoped_lock _scoped_lock(mutex);
      for (const auto &_conn : connections)
      {
        const auto &_net = _conn.second->metrics_->network_;
        total_read += _net.read_bytes_.accumulator_.load(std::memory_order_relaxed);
        total_write += _net.write_bytes_.accumulator_.load(std::memory_order_relaxed);
        total_read_per_minute += _net.read_bytes_.per_minute_.load(std::memory_order_relaxed);
        total_write_per_minute += _net.write_bytes_.per_minute_.load(std::memory_order_relaxed);
      }
    };
    _accumulate_metrics(
      state->tcp_connections_,
      state->tcp_connections_mutex_,
      _total_read,
      _total_write,
      _total_read_per_minute,
      _total_write_per_minute);
    _accumulate_metrics(
      state->agent_tcp_connections_,
      state->agent_tcp_connections_mutex_,
      _total_read,
      _total_write,
      _total_read_per_minute,
      _total_write_per_minute);
    _accumulate_metrics(
      state->unix_connections_,
      state->unix_connections_mutex_,
      _total_read,
      _total_write,
      _total_read_per_minute,
      _total_write_per_minute);
    _accumulate_metrics(
      state->agent_unix_connections_,
      state->agent_unix_connections_mutex_,
      _total_read,
      _total_write,
      _total_read_per_minute,
      _total_write_per_minute);
#endif

    const uint64_t _total_read_little = native_to_little(_total_read);
    std::memcpy(write_buffer.data() + _offset, &_total_read_little, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    const uint64_t _total_read_per_minute_little = native_to_little(_total_read_per_minute);
    std::memcpy(write_buffer.data() + _offset, &_total_read_per_minute_little, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    const uint64_t _total_write_little = native_to_little(_total_write);
    std::memcpy(write_buffer.data() + _offset, &_total_write_little, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    const uint64_t _total_write_per_minute_little = native_to_little(_total_write_per_minute);
    std::memcpy(write_buffer.data() + _offset, &_total_write_per_minute_little, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    uint64_t _total_keys = 0;
    uint64_t _total_counters = 0;
    uint64_t _total_buffers = 0;
    uint64_t _total_allocated_bytes_on_counters = 0;
    uint64_t _total_allocated_bytes_on_buffers = 0;

    for (const auto &_index = state->storage_.get<tag_by_key>(); auto &_item : _index)
    {
      if (_item.entry_.type_ == entry_types::counter)
      {
        _total_counters++;
        _total_allocated_bytes_on_counters += sizeof(value_type);
      }
      else
      {
        _total_buffers++;
        _total_allocated_bytes_on_buffers += _item.entry_.buffer_.load(std::memory_order_acquire)->size();
      }
      _total_keys++;
    }

    const uint64_t _total_keys_little = native_to_little(_total_keys);
    std::memcpy(write_buffer.data() + _offset, &_total_keys_little, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    const uint64_t _total_counters_little = native_to_little(_total_counters);
    std::memcpy(write_buffer.data() + _offset, &_total_counters_little, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    const uint64_t _total_buffers_little = native_to_little(_total_buffers);
    std::memcpy(write_buffer.data() + _offset, &_total_buffers_little, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    const uint64_t _total_allocated_bytes_on_counters_little = native_to_little(_total_allocated_bytes_on_counters);
    std::memcpy(write_buffer.data() + _offset, &_total_allocated_bytes_on_counters_little, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    const uint64_t _total_allocated_bytes_on_buffers_little = native_to_little(_total_allocated_bytes_on_buffers);
    std::memcpy(write_buffer.data() + _offset, &_total_allocated_bytes_on_buffers_little, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    uint64_t _total_subscriptions = 0;
    uint64_t _total_channels = 0;
    std::set<std::string, std::equal_to<>> _existing_keys;

    for (const auto &_index = state->subscriptions_->subscriptions_.get<by_channel_name>(); auto &_item : _index)
    {
      if (!_existing_keys.contains(_item.channel_))
      {
        _existing_keys.insert(_item.channel_);
        _total_channels++;
      }
      _total_subscriptions++;
    }

    const uint64_t _total_subscriptions_little = native_to_little(_total_subscriptions);
    std::memcpy(write_buffer.data() + _offset, &_total_subscriptions_little, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    const uint64_t _total_channels_little = native_to_little(_total_channels);
    std::memcpy(write_buffer.data() + _offset, &_total_channels_little, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    const uint64_t _started_at_little = native_to_little(state->started_at_);
    std::memcpy(write_buffer.data() + _offset, &_started_at_little, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    uint64_t _total_connections = 0;
    {
      std::scoped_lock _scoped_lock(
        state->unix_connections_mutex_,
        state->tcp_connections_mutex_,
        state->agent_unix_connections_mutex_,
        state->agent_tcp_connections_mutex_);
      _total_connections = state->tcp_connections_.size() + state->unix_connections_.size() +
                           state->agent_tcp_connections_.size() + state->agent_unix_connections_.size();
    }

    const uint64_t _total_connections_little = native_to_little(_total_connections);
    std::memcpy(write_buffer.data() + _offset, &_total_connections_little, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    const auto _version_data = get_version();
    std::memset(write_buffer.data() + _offset, 0, 16); // Limpia a cero
    for (std::size_t i = 0; i < std::min<std::size_t>(_version_data.size(), 16); ++i)
      write_buffer[_offset + i] = static_cast<std::byte>(_version_data[i]);

    batch.emplace_back(write_buffer.data() + _offset, 16);
    _offset += 16;

#ifndef NDEBUG
    fmt::println(
      "[{}] [{:%Y-%m-%d %H:%M:%S}] REQUEST INFO session_id={} RESPONSE ok=true",
      to_string(state->id_),
      std::chrono::system_clock::now(),
      to_string(id));
#endif
  }
} // namespace throttr
