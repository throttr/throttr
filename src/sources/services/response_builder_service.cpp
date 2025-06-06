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
#include <boost/endian/conversion.hpp>
#include <throttr/services/response_builder_service.hpp>
#include <throttr/services/subscriptions_service.hpp>

#include <throttr/connection.hpp>
#include <throttr/state.hpp>
#include <throttr/storage.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  inline void
  push_total_fragments(std::vector<boost::asio::const_buffer> &batch, std::vector<std::byte> &write_buffer, const uint64_t total)
  {
    const auto _offset = write_buffer.size();
    append_uint64_t(write_buffer, boost::endian::native_to_little(total));
    batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(total)));
  }

  std::size_t response_builder_service::write_list_entry_to_buffer(
    const std::shared_ptr<state> &state,
    std::vector<boost::asio::const_buffer> *batch,
    const entry_wrapper *entry,
    std::vector<std::byte> &write_buffer,
    const bool measure)
  {
    boost::ignore_unused(state);

    using namespace boost::endian;

    if (measure) // LCOV_EXCL_LINE Note: Partially tested.
      return entry->key_.size() + sizeof(value_type) + 11;

    const auto _offset = write_buffer.size();
    write_buffer.push_back(static_cast<std::byte>(entry->key_.size()));
    batch->emplace_back(boost::asio::buffer(&write_buffer[_offset], 1));
    batch->emplace_back(boost::asio::buffer(&entry->entry_.type_, sizeof(entry->entry_.type_)));
    batch->emplace_back(boost::asio::buffer(&entry->entry_.ttl_type_, sizeof(entry->entry_.ttl_type_)));

    // Handling _expires_at
    {
      const auto _expires_at = entry->entry_.expires_at_.load(std::memory_order_acquire);

      const auto _off = write_buffer.size();
      append_uint64_t(write_buffer, native_to_little(_expires_at));
      batch->emplace_back(boost::asio::buffer(&write_buffer[_off], sizeof(_expires_at)));
    }

    // Handling _bytes_used
    {
      value_type _bytes_used = 0;
      if (entry->entry_.type_ == entry_types::counter)
      {
        _bytes_used = sizeof(value_type);
      }
      else
      {
        const auto _buffer = entry->entry_.buffer_.load(std::memory_order_acquire);
        _bytes_used = static_cast<value_type>(_buffer->size());
      }
      const auto _scoped_offset = write_buffer.size();
      append_uint64_t(write_buffer, native_to_little(_bytes_used));
      batch->emplace_back(boost::asio::buffer(&write_buffer[_scoped_offset], sizeof(_bytes_used)));
    }

    return 0;
  }

  std::size_t response_builder_service::write_stats_entry_to_buffer(
    const std::shared_ptr<state> &state,
    std::vector<boost::asio::const_buffer> *batch,
    const entry_wrapper *entry,
    std::vector<std::byte> &write_buffer,
    const bool measure)
  {
    boost::ignore_unused(state);

    using namespace boost::endian;

    if (measure) // LCOV_EXCL_LINE Note: Partially tested.
      return entry->key_.size() + 1 + 8 * 4;

    const auto _offset = write_buffer.size();
    write_buffer.push_back(static_cast<std::byte>(entry->key_.size()));
    batch->emplace_back(boost::asio::buffer(&write_buffer[_offset], 1));

#ifdef ENABLED_FEATURE_METRICS
    for (const auto &_metric = *entry->metrics_; uint64_t _v :
                                                 {_metric.reads_per_minute_.load(std::memory_order_relaxed),
                                                  _metric.writes_per_minute_.load(std::memory_order_relaxed),
                                                  _metric.reads_accumulator_.load(std::memory_order_relaxed),
                                                  _metric.writes_accumulator_.load(std::memory_order_relaxed)})
    {
      const auto _off = write_buffer.size();
      append_uint64_t(write_buffer, native_to_little(_v));
      batch->emplace_back(boost::asio::buffer(&write_buffer[_off], sizeof(_v)));
    }
#else
    for (int _i = 0; _i < 4; ++_i)
    {
      constexpr uint64_t _empty = 0;
      const auto _off = write_buffer.size();
      append_uint64_t(write_buffer, native_to_little(_empty));
      batch->emplace_back(boost::asio::buffer(&write_buffer[_off], sizeof(_empty)));
    }
#endif

    return 0;
  }

  std::size_t response_builder_service::write_connections_entry_to_buffer(
    const std::shared_ptr<state> &state,
    std::vector<boost::asio::const_buffer> *batch,
    const connection<local_transport_socket> *conn,
    std::vector<std::byte> &write_buffer,
    bool measure)
  {
    boost::ignore_unused(state);

    if (measure)
      return 227;

    using namespace boost::endian;

    const auto _push = [&](const void *ptr, const std::size_t size) // NOSONAR
    {
      const auto _offset = write_buffer.size();
      const auto *_bytes = static_cast<const std::byte *>(ptr);
      write_buffer.insert(write_buffer.end(), _bytes, _bytes + size);
      batch->emplace_back(boost::asio::buffer(reinterpret_cast<const void *>(&write_buffer[_offset]), size)); // NOSONAR
    };

    // UUID (16 bytes)
    _push(conn->id_.data(), 16);

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
    const uint16_t _port = native_to_little(conn->port_);
    _push(&_port, sizeof(conn->port_));

#ifdef ENABLED_FEATURE_METRICS
    const auto &_metrics = *conn->metrics_;
#else
    constexpr uint64_t _empty = 0;
#endif

    for (const uint64_t _val :
         {conn->connected_at_,
#ifdef ENABLED_FEATURE_METRICS
          _metrics.network_.read_bytes_.accumulator_.load(std::memory_order_relaxed),
          _metrics.network_.write_bytes_.accumulator_.load(std::memory_order_relaxed),
          _metrics.network_.published_bytes_.accumulator_.load(std::memory_order_relaxed),
          _metrics.network_.received_bytes_.accumulator_.load(std::memory_order_relaxed),
          _metrics.memory_.allocated_bytes_.accumulator_.load(std::memory_order_relaxed),
          _metrics.memory_.consumed_bytes_.accumulator_.load(std::memory_order_relaxed)})
#else
          _empty,
          _empty,
          _empty,
          _empty,
          _empty,
          _empty})
#endif
    {
      const auto _offset = write_buffer.size();
      append_uint64_t(write_buffer, native_to_little(_val));
      batch->emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(uint64_t)));
    }

    constexpr std::array monitored_request_types =
      {request_types::insert,
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
       request_types::whoami};

    for (request_types type : monitored_request_types)
    {
      boost::ignore_unused(type);
#ifdef ENABLED_FEATURE_METRICS
      const auto &metric = conn->metrics_->commands_[static_cast<std::size_t>(type)];
      const uint64_t _value = metric.accumulator_.load(std::memory_order_relaxed);
#else
      constexpr uint64_t _value = 0;
#endif
      const auto _offset = write_buffer.size();
      append_uint64_t(write_buffer, native_to_little(_value));
      batch->emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(uint64_t)));
    }

    return 0;
  }

  void response_builder_service::handle_fragmented_connections_response(
    const std::shared_ptr<state> &state,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer)
  {
    batch.emplace_back(boost::asio::buffer(&state::success_response_, 1));

    std::vector<const connection<local_transport_socket> *> _fragment;
    std::vector<std::vector<const connection<local_transport_socket> *>> _fragments;
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

      // Convert _fragment_index and _connection_count to bytes
      for (const uint64_t _connection_count = _frag.size(); uint64_t val : {_fragment_index, _connection_count})
      {
        const auto _offset = write_buffer.size();
        append_uint64_t(write_buffer, boost::endian::native_to_little(val));
        batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(val)));
      }

      // Serialize connections in the fragment
      for (const auto *_conn : _frag)
      {
        write_connections_entry_to_buffer(state, &batch, _conn, write_buffer, false);
      }
    }
  }

  void response_builder_service::handle_fragmented_entries_response(
    const std::shared_ptr<state> &state,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer,
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

    // Insert fragment count
    {
      const auto _offset = write_buffer.size();
      const uint64_t _fragment_count = _fragments.size();
      append_uint64_t(write_buffer, boost::endian::native_to_little(_fragment_count));
      batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(_fragment_count)));
    }

    std::size_t _i = 0;
    for (const auto &_fragment : _fragments) // LCOV_EXCL_LINE Note: Partially tested.
    {
      uint64_t _fragment_index = _i + 1;
      uint64_t _key_count = _fragment.size();

      // Convert _fragment_index and _key_count to bytes and insert into the buffer
      for (const uint64_t _value : {_fragment_index, _key_count}) // LCOV_EXCL_LINE Note: Partially tested.
      {
        const auto _offset = write_buffer.size();
        append_uint64_t(write_buffer, boost::endian::native_to_little(_value));
        batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(_value)));
      }

      // Serialize entries
      for (const auto *_entry : _fragment) // LCOV_EXCL_LINE Note: Partially tested.
      {
        serialize_entry(&batch, _entry, false);
      }

      // Add key data to the batch
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
    std::vector<std::byte> &write_buffer)
  {
    batch.emplace_back(boost::asio::buffer(&state::success_response_, 1));

    std::vector<std::string> _channels_list;
    std::vector<std::vector<std::string>> _fragments;
    std::unordered_map<std::string, std::tuple<uint64_t, uint64_t, uint64_t>> _channel_stats; // NOSONAR
    {
      const auto &_subs = state->subscriptions_->subscriptions_.get<by_channel_name>();
      uint64_t _read_sum = 0;
      uint64_t _write_sum = 0;
      uint64_t _count = 0;
      std::size_t _fragment_size = 0;
      std::vector<std::string> _current_fragment;

      for (auto _it = _subs.begin(); _it != _subs.end();) // LCOV_EXCL_LINE Note: Partially tested.
      {
        std::string _current_channel{_it->channel_};
        _read_sum = 0;
        _write_sum = 0;
        _count = 0;

        auto _range = _subs.equal_range(_current_channel);                           // NOSONAR
        for (auto _range_it = _range.first; _range_it != _range.second; ++_range_it) // LCOV_EXCL_LINE Note: Partially tested.
        {
#ifdef ENABLED_FEATURE_METRICS
          _read_sum += _range_it->metrics_->read_bytes_.accumulator_.load(std::memory_order_relaxed);
          _write_sum += _range_it->metrics_->write_bytes_.accumulator_.load(std::memory_order_relaxed);
#else
          _read_sum += 0;
          _write_sum += 0;
#endif
          ++_count;
        }

        _channel_stats[_current_channel] = {_read_sum, _write_sum, _count};
        constexpr std::size_t _channel_size = 1 + 8 + 8 + 8;
        // LCOV_EXCL_START
        if (_fragment_size + _channel_size > 2048)
        {
          _fragments.push_back(_current_fragment);
          _current_fragment.clear();
          _fragment_size = 0;
        }
        // LCOV_EXCL_STOP

        _current_fragment.push_back(_current_channel);
        _fragment_size += _channel_size;

        _it = _range.second;
      }

      if (!_current_fragment.empty()) // LCOV_EXCL_LINE Note: Partially tested.
        _fragments.push_back(_current_fragment);
    }

    push_total_fragments(batch, write_buffer, _fragments.size());

    std::size_t _index = 1;
    for (const auto &_frag : _fragments)
    {
      uint64_t _fragment_index = _index;
      _index++;

      // Convert _fragment_index and _channel_count to bytes using std::array
      for (const uint64_t _channel_count = _frag.size(); uint64_t _val : {_fragment_index, _channel_count})
      {
        const auto _offset = write_buffer.size();
        append_uint64_t(write_buffer, boost::endian::native_to_little(_val));
        batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(_val)));
      }

      // Serialize channels in the fragment
      for (const auto &_chan : _frag)
      {
        const auto &[_read, _write, _count] = _channel_stats[_chan];
        const auto _size = static_cast<std::uint8_t>(_chan.size());

        const auto _offset = write_buffer.size();
        write_buffer.push_back(std::byte{_size});
        batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], 1));

        for (const uint64_t _metric : {_read, _write, _count})
        {
          const auto _scoped_offset = write_buffer.size();
          append_uint64_t(write_buffer, boost::endian::native_to_little(_metric));
          batch.emplace_back(boost::asio::buffer(&write_buffer[_scoped_offset], sizeof(_metric)));
        }
      }

      for (const auto &_channel : _frag)
      {
        const auto _offset = write_buffer.size();
        std::ranges::transform(_channel, std::back_inserter(write_buffer), [](char c) { return static_cast<std::byte>(c); });
        batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], _channel.size()));
      }
    }
  }
} // namespace throttr
