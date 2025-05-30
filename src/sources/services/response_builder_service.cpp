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

#include <boost/core/ignore_unused.hpp>
#include <throttr/services/response_builder_service.hpp>

#include <throttr/connection.hpp>
#include <throttr/state.hpp>
#include <throttr/storage.hpp>

namespace throttr
{
  inline void push_total_fragments(
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::uint8_t> &write_buffer,
    const uint64_t total)
  {
    const auto _offset = write_buffer.size();
    const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&total); // NOSONAR
    write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(total));
    batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(total)));
  }

  std::size_t response_builder_service::write_list_entry_to_buffer(
    const std::shared_ptr<state> &state,
    std::vector<boost::asio::const_buffer> *batch,
    const entry_wrapper *entry,
    std::vector<std::uint8_t> &write_buffer,
    const bool measure)
  {
    boost::ignore_unused(state);

    if (measure) // LCOV_EXCL_LINE Note: Partially tested.
      return entry->key_.size() + entry->entry_.value_.size() + 11;

    const auto _offset = write_buffer.size();
    write_buffer.push_back(static_cast<std::uint8_t>(entry->key_.size()));
    batch->emplace_back(boost::asio::buffer(&write_buffer[_offset], 1));
    batch->emplace_back(boost::asio::buffer(&entry->entry_.type_, sizeof(entry->entry_.type_)));
    batch->emplace_back(boost::asio::buffer(&entry->entry_.ttl_type_, sizeof(entry->entry_.ttl_type_)));

    {
      const auto _expires_at =
        std::chrono::duration_cast<std::chrono::nanoseconds>(entry->entry_.expires_at_.time_since_epoch()).count();
      const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&_expires_at); // NOSONAR
      const auto _off = write_buffer.size();
      write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(_expires_at));
      batch->emplace_back(boost::asio::buffer(&write_buffer[_off], sizeof(_expires_at)));
    }

    {
      const auto _bytes_used = static_cast<value_type>(entry->entry_.value_.size());
      const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&_bytes_used); // NOSONAR
      const auto _off = write_buffer.size();
      write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(_bytes_used));
      batch->emplace_back(boost::asio::buffer(&write_buffer[_off], sizeof(_bytes_used)));
    }

    return 0;
  }

  std::size_t response_builder_service::write_stats_entry_to_buffer(
    const std::shared_ptr<state> &state,
    std::vector<boost::asio::const_buffer> *batch,
    const entry_wrapper *entry,
    std::vector<std::uint8_t> &write_buffer,
    const bool measure)
  {
    boost::ignore_unused(state);

    if (measure) // LCOV_EXCL_LINE Note: Partially tested.
      return entry->key_.size() + 1 + 8 * 4;

    const auto _offset = write_buffer.size();
    write_buffer.push_back(static_cast<std::uint8_t>(entry->key_.size()));
    batch->emplace_back(boost::asio::buffer(&write_buffer[_offset], 1));

    for (const auto &_metric = *entry->metrics_; uint64_t _v :
                                                 {_metric.reads_per_minute_.load(std::memory_order_relaxed),
                                                  _metric.writes_per_minute_.load(std::memory_order_relaxed),
                                                  _metric.reads_accumulator_.load(std::memory_order_relaxed),
                                                  _metric.writes_accumulator_.load(std::memory_order_relaxed)})
    {
      const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&_v); // NOSONAR
      const auto _off = write_buffer.size();
      write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(_v));
      batch->emplace_back(boost::asio::buffer(&write_buffer[_off], sizeof(_v)));
    }

    return 0;
  }

  std::size_t response_builder_service::write_connections_entry_to_buffer(
    const std::shared_ptr<state> &state,
    std::vector<boost::asio::const_buffer> *batch,
    const connection *conn,
    std::vector<std::uint8_t> &write_buffer,
    bool measure)
  {
    boost::ignore_unused(state);

    if (measure)
      return 227;

    const auto _push = [&](const void *ptr, const std::size_t size) // NOSONAR
    {
      const auto _offset = write_buffer.size();
      const auto *_bytes = static_cast<const std::uint8_t *>(ptr);
      write_buffer.insert(write_buffer.end(), _bytes, _bytes + size);
      batch->emplace_back(boost::asio::buffer(&write_buffer[_offset], size));
    };

    // UUID (16 bytes)
    _push(conn->id_.data, 16);

    // IP version (1 byte)
    const std::uint8_t _ip_version = conn->ip_.contains(':') ? 0x06 : 0x04;
    _push(&_ip_version, 1);

    // IP padded to 16 bytes
    std::array<std::uint8_t, 16> _ip_bytes = {};
    const auto &_ip_str = conn->ip_;
    const auto _len = std::min<std::size_t>(_ip_str.size(), 16);
    std::memcpy(_ip_bytes.data(), _ip_str.data(), _len);
    _push(_ip_bytes.data(), 16);

    // Port (2 bytes)
    _push(&conn->port_, sizeof(conn->port_));

#ifdef ENABLED_FEATURE_METRICS
    const auto &_metrics = *conn->metrics_;

    for (uint64_t val :
         {conn->connected_at_,
          _metrics.network_.read_bytes_.accumulator_.load(std::memory_order_relaxed),
          _metrics.network_.write_bytes_.accumulator_.load(std::memory_order_relaxed),
          _metrics.network_.published_bytes_.accumulator_.load(std::memory_order_relaxed),
          _metrics.network_.received_bytes_.accumulator_.load(std::memory_order_relaxed),
          _metrics.memory_.allocated_bytes_.accumulator_.load(std::memory_order_relaxed),
          _metrics.memory_.consumed_bytes_.accumulator_.load(std::memory_order_relaxed)
         })
    {
      _push(&val, sizeof(val));
    }

    constexpr std::array monitored_request_types = {
      request_types::insert,
      request_types::set,
      request_types::query,
      request_types::get,
      request_types::update,
      request_types::purge,
      request_types::list,
      request_types::info,
      request_types::stat,
      request_types::stats,
      request_types::publish,
      request_types::subscribe,
      request_types::unsubscribe,
      request_types::connections,
      request_types::connection,
      request_types::channels,
      request_types::channel,
      request_types::whoami
    };

    for (request_types type : monitored_request_types)
    {
      const auto &metric = conn->metrics_->commands_[static_cast<std::size_t>(type)];
      const uint64_t value = metric.accumulator_.load(std::memory_order_relaxed);
      _push(&value, sizeof(value));
    }


#else
    // Rellenar con ceros si las métricas no están habilitadas
    std::array<std::uint8_t, 227 - 16 - 1 - 16 - 2> zeros = {};
    _push(zeros.data(), zeros.size());
#endif

    return 0;
  }

  void response_builder_service::handle_fragmented_connections_response(
    const std::shared_ptr<state> &state,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::uint8_t> &write_buffer)
  {
#ifndef ENABLED_FEATURE_METRICS
    batch.emplace_back(boost::asio::buffer(&failed_response_, 1));
    return;
#endif
    batch.emplace_back(boost::asio::buffer(&state::success_response_, 1));

    std::vector<const connection *> _fragment;
    std::vector<std::vector<const connection *>> _fragments;
    {
      std::size_t _current_fragment_size = 0;
      std::lock_guard _lock(state->connections_mutex_);
      for (const auto &[_id, _conn] : state->connections_)
      {
        const std::size_t _conn_size = write_connections_entry_to_buffer(state, nullptr, _conn, write_buffer, true);

        // LCOV_EXCL_START
        // @Pending this should be tested but requires a mechanism to create a huge amount of connections
        if (constexpr std::size_t _max_fragment_size = 2048; _current_fragment_size + _conn_size > _max_fragment_size)
        {
          _fragments.push_back(std::move(_fragment));
          _fragment.clear();
          _current_fragment_size = 0;
        }
        // LCOV_EXCL_STOP

        _fragment.push_back(_conn);
        _current_fragment_size += _conn_size;
      }
    }

    if (!_fragment.empty()) // LCOV_EXCL_LINE Note: Partially tested.
      _fragments.push_back(std::move(_fragment));

    push_total_fragments(batch, write_buffer, _fragments.size());

    std::size_t _index = 1;
    for (const auto &_frag : _fragments)
    {
      const uint64_t _fragment_index = _index;
      ++_index;

      for (const uint64_t _connection_count = _frag.size(); uint64_t val : {_fragment_index, _connection_count})
      {
        const auto _offset = write_buffer.size();
        const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&val); // NOSONAR
        write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(val));
        batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(val)));
      }

      for (const auto *_conn : _frag)
      {
        write_connections_entry_to_buffer(state, &batch, _conn, write_buffer, false);
      }
    }
  }

  void response_builder_service::handle_fragmented_entries_response(
    const std::shared_ptr<state> &state,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::uint8_t> &write_buffer,
    const std::size_t max_fragment_size,
    const std::function<std::size_t(std::vector<boost::asio::const_buffer> *, const entry_wrapper *, bool)> &serialize_entry)
  {
    const auto &_index = state->storage_.get<tag_by_key>();
    std::size_t _fragments_count = 1;
    std::size_t _fragment_size = 0;

    batch.emplace_back(boost::asio::buffer(&state::success_response_, 1));

    std::vector<const entry_wrapper *> _fragment_items;
    std::vector<std::vector<const entry_wrapper *>> _fragments;

    for (auto &_item : _index) // LCOV_EXCL_LINE Note: Partially tested.
    {
      // LCOV_EXCL_START
      if (_item.expired_)
        continue;
        // LCOV_EXCL_STOP

#ifdef ENABLED_FEATURE_METRICS
      _item.metrics_->reads_.fetch_add(1, std::memory_order_relaxed);
#endif

      const std::size_t _item_size = serialize_entry(nullptr, &_item, true);
      if (_fragment_size + _item_size > max_fragment_size) // LCOV_EXCL_LINE Note: Partially tested.
      {
        _fragments.push_back(_fragment_items);
        _fragment_size = 0;
        _fragments_count++;
        _fragment_items.clear();
      }

      _fragment_items.emplace_back(&_item);
      _fragment_size += _item_size;
    }

    if (!_fragment_items.empty()) // LCOV_EXCL_LINE Note: Partially tested.
    {
      _fragments.push_back(std::move(_fragment_items));
    }

    {
      const auto _offset = write_buffer.size();
      const uint64_t _fragment_count = _fragments.size();
      const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&_fragment_count); // NOSONAR
      write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(_fragment_count));
      batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(_fragment_count)));
    }

    std::size_t _i = 0;
    for (const auto &_fragment : _fragments) // LCOV_EXCL_LINE Note: Partially tested.
    {
      const uint64_t _fragment_index = _i + 1;
      const uint64_t _key_count = _fragment.size();

      for (uint64_t value : {_fragment_index, _key_count}) // LCOV_EXCL_LINE Note: Partially tested.
      {
        const auto _offset = write_buffer.size();
        const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&value); // NOSONAR
        write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(value));
        batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(value)));
      }

      for (const auto *_entry : _fragment) // LCOV_EXCL_LINE Note: Partially tested.
      {
        serialize_entry(&batch, _entry, false);
      }

      for (const auto &_entry : _fragment) // LCOV_EXCL_LINE Note: Partially tested.
      {
        batch.emplace_back(boost::asio::buffer(_entry->key_.data(), _entry->key_.size()));
      }

      _i++;
    }
  }

  void response_builder_service::handle_fragmented_channels_response(
    const std::shared_ptr<state> &state,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::uint8_t> &write_buffer)
  {
#ifndef ENABLED_FEATURE_METRICS
    batch.emplace_back(boost::asio::buffer(&failed_response_, 1));
    return;
#endif

    batch.emplace_back(boost::asio::buffer(&state::success_response_, 1));

    std::vector<std::string_view> _channels_list;
    std::vector<std::vector<std::string_view>> _fragments;
    std::unordered_map<std::string_view, std::tuple<uint64_t, uint64_t, uint64_t>> _channel_stats;
    {
      const auto &_subs = state->subscriptions_->subscriptions_.get<by_channel_name>();
      uint64_t _read_sum = 0;
      uint64_t _write_sum = 0;
      uint64_t _count = 0;
      std::size_t _fragment_size = 0;
      std::vector<std::string_view> _current_fragment;

      for (auto it = _subs.begin(); it != _subs.end();) // LCOV_EXCL_LINE Note: Partially tested.
      {
        std::string_view _current_channel = it->channel();
        _read_sum = 0;
        _write_sum = 0;
        _count = 0;

        auto range = _subs.equal_range(_current_channel); // NOSONAR
        for (auto range_it = range.first; range_it != range.second; ++range_it) // LCOV_EXCL_LINE Note: Partially tested.
        {
          _read_sum += range_it->metrics_.read_bytes_.accumulator_.load(std::memory_order_relaxed);
          _write_sum += range_it->metrics_.write_bytes_.accumulator_.load(std::memory_order_relaxed);
          ++_count;
        }

        _channel_stats[_current_channel] = {_read_sum, _write_sum, _count};
        constexpr std::size_t _channel_size = 1 + 8 + 8 + 8;
        // LCOV_EXCL_START
        if (_fragment_size + _channel_size > 2048)
        {
          _fragments.push_back(std::move(_current_fragment));
          _current_fragment.clear();
          _fragment_size = 0;
        }
        // LCOV_EXCL_STOP

        _current_fragment.push_back(_current_channel);
        _fragment_size += _channel_size;

        it = range.second;
      }

      if (!_current_fragment.empty()) // LCOV_EXCL_LINE Note: Partially tested.
        _fragments.push_back(std::move(_current_fragment));
    }

    push_total_fragments(batch, write_buffer, _fragments.size());

    std::size_t _index = 1;
    for (const auto &_frag : _fragments)
    {
      uint64_t _fragment_index = _index;
      _index++;

      for (const uint64_t _channel_count = _frag.size(); uint64_t val : {_fragment_index, _channel_count})
      {
        const auto _offset = write_buffer.size();
        const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&val); // NOSONAR
        write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(val));
        batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(val)));
      }

      for (const auto &_chan : _frag)
      {
        const auto &[read, write, count] = _channel_stats[_chan];
        const auto _size = static_cast<std::uint8_t>(_chan.size());

        const auto _offset1 = write_buffer.size();
        write_buffer.push_back(_size);
        batch.emplace_back(boost::asio::buffer(&write_buffer[_offset1], 1));

        for (uint64_t metric : {read, write, count})
        {
          const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&metric); // NOSONAR
          const auto _off = write_buffer.size();
          write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(metric));
          batch.emplace_back(boost::asio::buffer(&write_buffer[_off], sizeof(metric)));
        }
      }

      for (const auto &_chan : _frag)
      {
        batch.emplace_back(boost::asio::buffer(_chan.data(), _chan.size()));
      }
    }
  }
} // namespace throttr
