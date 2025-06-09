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

    batch.emplace_back(&state::success_response_, 1);

    const auto _now =
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    boost::ignore_unused(_now);

    const auto push_u64 = [&](const uint64_t value)
    {
      const auto _offset = write_buffer.size();
      append_uint64_t(write_buffer, native_to_little(value));
      batch.emplace_back(&write_buffer[_offset], sizeof(value));
    };

    uint64_t _total_requests = 0;
    uint64_t _total_requests_per_minute = 0;
    push_u64(static_cast<uint64_t>(_now));
#ifdef ENABLED_FEATURE_METRICS
    const auto &metrics = state->metrics_collector_->commands_;
    for (const auto &m : metrics)
    {
      _total_requests += m.accumulator_.load(std::memory_order_relaxed);
      _total_requests_per_minute += m.per_minute_.load(std::memory_order_relaxed);
    }
#endif
    push_u64(_total_requests);
    push_u64(_total_requests_per_minute);

    for (auto metric_type : {
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
         })
    {
#ifdef ENABLED_FEATURE_METRICS
      push_u64(metrics[static_cast<std::size_t>(metric_type)].accumulator_.load(std::memory_order_relaxed));
      push_u64(metrics[static_cast<std::size_t>(metric_type)].per_minute_.load(std::memory_order_relaxed));
#else
      boost::ignore_unused(metric_type);
      push_u64(_total_requests);
      push_u64(_total_requests);
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
      state->unix_connections_,
      state->unix_connections_mutex_,
      _total_read,
      _total_write,
      _total_read_per_minute,
      _total_write_per_minute);
#endif

    push_u64(_total_read);
    push_u64(_total_read_per_minute);
    push_u64(_total_write);
    push_u64(_total_write_per_minute);

    uint64_t _total_keys = 0;
    uint64_t _total_counters = 0;
    uint64_t _total_buffers = 0;
    uint64_t _total_allocated_bytes_on_counters = 0;
    uint64_t _total_allocated_bytes_on_buffers = 0;

    for (const auto &_index = state->storage_.get<tag_by_key>(); auto &_item : _index) // LCOV_EXCL_LINE Note: Partially tested.
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

    push_u64(_total_keys);
    push_u64(_total_counters);
    push_u64(_total_buffers);
    push_u64(_total_allocated_bytes_on_counters);
    push_u64(_total_allocated_bytes_on_buffers);

    uint64_t _total_subscriptions = 0;
    uint64_t _total_channels = 0;
    std::set<std::string> _existing_keys; // NOSONAR

    for (const auto &_index =                                          // LCOV_EXCL_LINE Note: Partially tested.
         state->subscriptions_->subscriptions_.get<by_channel_name>(); // LCOV_EXCL_LINE Note: Partially tested.
         auto &_item : _index)                                         // LCOV_EXCL_LINE Note: Partially tested.
    {
      if (!_existing_keys.contains(_item.channel_)) // LCOV_EXCL_LINE Note: Partially tested.
      {
        _existing_keys.insert(_item.channel_);
        _total_channels++;
      }
      _total_subscriptions++;
    }

    push_u64(_total_subscriptions);
    push_u64(_total_channels);
    push_u64(state->started_at_);

    uint64_t _total_connections = 0;
    {
      std::scoped_lock _scoped_lock(state->unix_connections_mutex_, state->tcp_connections_mutex_);
      _total_connections = state->tcp_connections_.size() + state->unix_connections_.size();
    }
    push_u64(_total_connections);

    {
      const auto _offset = write_buffer.size();
      write_buffer.resize(_offset + 16, std::byte{0});
      const auto _version_data = get_version();

      for (std::size_t i = 0; i < std::min<std::size_t>(_version_data.size(), 16); ++i)
        write_buffer[_offset + i] = static_cast<std::byte>(_version_data[i]);

      batch.emplace_back(&write_buffer[_offset], 16);
    }

    // LCOV_EXCL_START
#ifndef NDEBUG
    fmt::println(
      "{:%Y-%m-%d %H:%M:%S} REQUEST INFO session_id={} RESPONSE ok=true", std::chrono::system_clock::now(), to_string(id));
#endif
    // LCOV_EXCL_STOP
  }
} // namespace throttr
