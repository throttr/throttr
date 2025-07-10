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
    batch.emplace_back(&write_buffer[_offset], sizeof(total));
  }

  std::pair<std::size_t, std::size_t> response_builder_service::write_list_entry_to_buffer(
    const std::shared_ptr<state> &state,
    std::vector<boost::asio::const_buffer> *batch,
    const entry_wrapper *entry,
    std::vector<std::byte> &write_buffer,
    std::size_t &offset,
    const bool measure)
  {
    boost::ignore_unused(state);

    using namespace boost::endian;

    if (measure)
      return {17, 5}; // Key size + Expires At + Bytes used | number of batch emplace back

    const auto _key_length = static_cast<std::byte>(entry->key_.size());
    std::memcpy(write_buffer.data() + offset, &_key_length, sizeof(uint8_t));
    batch->emplace_back(write_buffer.data() + offset, sizeof(uint8_t));
    offset++;

    batch->emplace_back(&entry->entry_.type_, sizeof(uint8_t));
    batch->emplace_back(&entry->entry_.ttl_type_, sizeof(uint8_t));

    // Handling _expires_at
    {
      const auto _expires_at = native_to_little(entry->entry_.expires_at_.load(std::memory_order_acquire));
      std::memcpy(write_buffer.data() + offset, &_expires_at, sizeof(uint64_t));
      batch->emplace_back(write_buffer.data() + offset, sizeof(uint64_t));
      offset += sizeof(uint64_t);
    }

    // Handling _bytes_used
    {
      if (entry->entry_.type_ == entry_types::counter)
      {
        value_type _bytes_used = native_to_little(sizeof(value_type));

        std::memcpy(write_buffer.data() + offset, &_bytes_used, sizeof(value_type));
        batch->emplace_back(write_buffer.data() + offset, sizeof(value_type));
        offset += sizeof(value_type);
      }
      else
      {
        const auto _buffer = entry->entry_.buffer_.load(std::memory_order_acquire);
        value_type _bytes_used = native_to_little(_buffer->size());

        std::memcpy(write_buffer.data() + offset, &_bytes_used, sizeof(value_type));
        batch->emplace_back(write_buffer.data() + offset, sizeof(value_type));
        offset += sizeof(value_type);
      }
    }

    return {0, 0};
  }

  std::pair<std::size_t, std::size_t> response_builder_service::write_stats_entry_to_buffer(
    const std::shared_ptr<state> &state,
    std::vector<boost::asio::const_buffer> *batch,
    const entry_wrapper *entry,
    std::vector<std::byte> &write_buffer,
    std::size_t &offset,
    const bool measure)
  {
    boost::ignore_unused(state);

    using namespace boost::endian;

    if (measure) // LCOV_EXCL_LINE Note: Partially tested.
      return {33, 5};

    const auto _key_length = static_cast<std::byte>(entry->key_.size());
    std::memcpy(write_buffer.data() + offset, &_key_length, sizeof(uint8_t));
    batch->emplace_back(write_buffer.data() + offset, sizeof(uint8_t));
    offset++;

#ifdef ENABLED_FEATURE_METRICS
    for (const auto &_metric = *entry->metrics_; uint64_t _v :
                                                 {native_to_little(_metric.reads_per_minute_.load(std::memory_order_relaxed)),
                                                  native_to_little(_metric.writes_per_minute_.load(std::memory_order_relaxed)),
                                                  native_to_little(_metric.reads_accumulator_.load(std::memory_order_relaxed)),
                                                  native_to_little(_metric.writes_accumulator_.load(std::memory_order_relaxed))})
    {
      std::memcpy(write_buffer.data() + offset, &_v, sizeof(uint64_t));
      batch->emplace_back(write_buffer.data() + offset, sizeof(uint64_t));
      offset += sizeof(uint64_t);
    }
#else
    constexpr uint64_t _empty = 0;
    for (int _i = 0; _i < 4; ++_i)
    {
      std::memcpy(write_buffer.data() + offset, &_empty, sizeof(uint64_t));
      batch->emplace_back(write_buffer.data() + offset, sizeof(uint64_t));
      offset += sizeof(uint64_t);
    }
#endif

    return {0, 0};
  }

  void response_builder_service::handle_fragmented_connections_response(
    const std::shared_ptr<state> &state,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer)
  {
    using fragment_container =
      std::tuple<std::vector<const connection<tcp_socket> *>, std::vector<const connection<unix_socket> *>>;

    fragment_container _fragment;
    std::vector<fragment_container> _fragments;
    std::size_t _current_fragment_size = 0;
    constexpr std::size_t _max_fragment_size = 2048; // LCOV_EXCL_LINE

    std::size_t _global_batch_size = 2;        // This is status + fragments count
    std::size_t _global_write_buffer_size = 8; // This is fragment count

    std::size_t _offset = 0;

    auto _fragmenter = [&](auto &connections, auto &mutex)
    {
      std::lock_guard _lock(mutex);
      for (const auto &[_id, _conn] : connections)
      {
        const auto [_conn_size, _batch_size] =
          write_connections_entry_to_buffer(state, nullptr, _conn, write_buffer, _offset, true);

        _global_batch_size += _batch_size;       // + 31
        _global_write_buffer_size += _conn_size; // + 237

        if (_current_fragment_size + _conn_size > _max_fragment_size)
        {
          _fragments.push_back(std::move(_fragment));
          std::get<0>(_fragment).clear();
          std::get<1>(_fragment).clear();
          _current_fragment_size = 0;
        }

        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(_conn)>, connection<tcp_socket> *>)
          std::get<0>(_fragment).push_back(_conn);
        else
          std::get<1>(_fragment).push_back(_conn);
        _current_fragment_size += _conn_size;
      }
    };

    _fragmenter(state->tcp_connections_, state->tcp_connections_mutex_);
    _fragmenter(state->unix_connections_, state->unix_connections_mutex_);
    _fragmenter(state->agent_tcp_connections_, state->agent_tcp_connections_mutex_);
    _fragmenter(state->agent_unix_connections_, state->agent_unix_connections_mutex_);

    if (!std::get<0>(_fragment).empty() || !std::get<1>(_fragment).empty()) // LCOV_EXCL_LINE Note: Partially tested.
      _fragments.push_back(std::move(_fragment));

    const auto _fragments_count = boost::endian::native_to_little(_fragments.size());

    // per fragment index + connections in fragment count
    _global_batch_size += _fragments.size() * 2;
    _global_write_buffer_size += _fragments.size() * sizeof(uint64_t) * 2;

    batch.reserve(batch.size() + _global_batch_size);
    write_buffer.resize(write_buffer.size() + _global_write_buffer_size);

    batch.emplace_back(&state::success_response_, 1);

    std::memcpy(write_buffer.data() + _offset, &_fragments_count, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t);

    std::size_t _index = 1;
    for (const auto &[_tcp_fragment, _unix_fragment] : _fragments)
    {
      const uint64_t _fragment_index = _index;
      ++_index;

      for (const auto _connection_count = _tcp_fragment.size() + _unix_fragment.size();
           const uint64_t _val :
           {boost::endian::native_to_little(_fragment_index), boost::endian::native_to_little(_connection_count)})
      {
        std::memcpy(write_buffer.data() + _offset, &_val, sizeof(uint64_t));
        batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
        _offset += sizeof(uint64_t);
      }

      for (const auto *_conn : _tcp_fragment)
      {
        write_connections_entry_to_buffer<tcp_socket>(state, &batch, _conn, write_buffer, _offset, false);
      }
      for (const auto *_conn : _unix_fragment)
      {
        write_connections_entry_to_buffer<unix_socket>(state, &batch, _conn, write_buffer, _offset, false);
      }
    }
  }

  void response_builder_service::handle_fragmented_entries_response(
    const std::shared_ptr<state> &state,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer,
    const std::size_t max_fragment_size,
    const std::function<
      std::pair<std::size_t, std::size_t>(std::vector<boost::asio::const_buffer> *, const entry_wrapper *, std::size_t &, bool)>
      &serialize_entry)
  {
    const auto &_index = state->storage_.get<tag_by_key>();
    std::size_t _fragments_count = 1;
    std::size_t _fragment_size = 0;

    std::size_t _offset = 0;

    std::vector<const entry_wrapper *> _fragment_items;
    std::vector<std::vector<const entry_wrapper *>> _fragments;

    std::size_t _buffer_required_size = 0;
    std::size_t _batch_required_size = 2; // status + fragment count

    for (auto &_item : _index) // LCOV_EXCL_LINE Note: Partially tested.
    {
      // LCOV_EXCL_START
      if (_item.expired_)
        continue;
        // LCOV_EXCL_STOP

#ifdef ENABLED_FEATURE_METRICS
      _item.metrics_->reads_.fetch_add(1, std::memory_order_relaxed);
#endif

      const auto [_item_size, _item_batch_size] = serialize_entry(nullptr, &_item, _offset, true);
      if (_fragment_size + _item_size > max_fragment_size) // LCOV_EXCL_LINE Note: Partially tested.
      {
        _fragments.push_back(_fragment_items);
        _fragment_size = 0;
        _fragments_count++;
        _fragment_items.clear();
        _batch_required_size += 2; // fragment index + keys count
      }

      _fragment_items.emplace_back(&_item);
      _fragment_size += _item_size + _item.key_.size();

      _buffer_required_size += _item_size;
      _batch_required_size += _item_batch_size;
    }

    if (!_fragment_items.empty()) // LCOV_EXCL_LINE Note: Partially tested.
    {
      _fragments.push_back(std::move(_fragment_items));
    }

    _batch_required_size += _fragments.size() * 2;

    // buffer count + index per fragment + keys fragment
    _buffer_required_size += sizeof(std::uint64_t) + _fragments.size() * (sizeof(std::uint64_t) * 2);

    batch.reserve(batch.size() + _batch_required_size);
    write_buffer.resize(write_buffer.size() + _buffer_required_size);

    batch.emplace_back(&state::success_response_, 1);

    // Insert fragment count
    {
      const uint64_t _fragment_count = boost::endian::native_to_little(_fragments.size());
      std::memcpy(write_buffer.data() + _offset, &_fragment_count, sizeof(uint64_t));
      batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
      _offset += sizeof(uint64_t);
    }

    std::size_t _i = 0;
    for (const auto &_fragment : _fragments) // LCOV_EXCL_LINE Note: Partially tested.
    {
      uint64_t _fragment_index = _i + 1;
      uint64_t _key_count = _fragment.size();

      // Convert _fragment_index and _key_count to bytes and insert into the buffer
      for (const uint64_t _value : {_fragment_index, _key_count}) // LCOV_EXCL_LINE Note: Partially tested.
      {
        const auto _scoped_value = boost::endian::native_to_little(_value);
        std::memcpy(write_buffer.data() + _offset, &_scoped_value, sizeof(uint64_t));
        batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
        _offset += sizeof(uint64_t);
      }

      // Serialize entries
      for (const auto *_entry : _fragment) // LCOV_EXCL_LINE Note: Partially tested.
      {
        serialize_entry(&batch, _entry, _offset, false);
      }

      // Add key data to the batch
      for (const auto &_entry : _fragment) // LCOV_EXCL_LINE Note: Partially tested.
      {
        batch.emplace_back(_entry->key_.data(), _entry->key_.size());
      }

      _i++;
    }
  }

  void response_builder_service::handle_fragmented_channels_response(
    const std::shared_ptr<state> &state,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer)
  {
    std::size_t _offset = 0;

    std::vector<std::string> _channels_list;
    std::vector<std::vector<std::string>> _fragments;
    std::unordered_map<std::string, std::tuple<uint64_t, uint64_t, uint64_t>> _channel_stats; // NOSONAR

    std::size_t _write_buffer_size = 0;
    std::size_t _channels_count = 0;

    {
      const auto &_subs = state->subscriptions_->subscriptions_.get<by_channel_name>();
      uint64_t _read_sum = 0;
      uint64_t _write_sum = 0;
      uint64_t _count = 0;
      std::size_t _fragment_size = 0;
      std::vector<std::string> _current_fragment;

      _channels_count = _subs.size();

      for (auto _it = _subs.begin(); _it != _subs.end();) // LCOV_EXCL_LINE Note: Partially tested.
      {
        std::string _current_channel{_it->channel_};
        _read_sum = 0;
        _write_sum = 0;
        _count = 0;

        _write_buffer_size += sizeof(uint64_t) * 3 + 1 + _current_channel.size();

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
          _write_buffer_size += sizeof(uint64_t) * 2;
        }
        // LCOV_EXCL_STOP

        _current_fragment.push_back(_current_channel);
        _fragment_size += _channel_size;

        _it = _range.second;
      }

      if (!_current_fragment.empty())
      {
        _fragments.push_back(_current_fragment);
        _write_buffer_size += sizeof(uint64_t) * 2;
      }
    }

    batch.reserve(batch.size() + 1 + 2 * _fragments.size() + 3 * _channels_count);
    write_buffer.resize(write_buffer.size() + _write_buffer_size);

    batch.emplace_back(&state::success_response_, 1);

    const auto _fragment_count_native = boost::endian::native_to_little(_fragments.size());
    std::memcpy(write_buffer.data() + _offset, &_fragment_count_native, sizeof(uint64_t));
    batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
    _offset += sizeof(uint64_t); // 8 bytes

    std::size_t _index = 1;
    for (const auto &_frag : _fragments)
    {
      uint64_t _fragment_index = _index;
      _index++;

      // Convert _fragment_index and _channel_count to bytes using std::array
      for (const uint64_t _channel_count = _frag.size(); const uint64_t _val : {_fragment_index, _channel_count})
      {
        const auto _fragment_metadata_native = boost::endian::native_to_little(_val);
        std::memcpy(write_buffer.data() + _offset, &_fragment_metadata_native, sizeof(uint64_t));
        batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
        _offset += sizeof(uint64_t); // 8 bytes
      }

      // Serialize channels in the fragment
      for (const auto &_chan : _frag)
      {
        const auto &[_read, _write, _count] = _channel_stats[_chan];
        const auto _size = static_cast<std::uint8_t>(_chan.size());

        const auto _channel_size = std::byte{_size};
        std::memcpy(write_buffer.data() + _offset, &_channel_size, sizeof(uint8_t));
        batch.emplace_back(write_buffer.data() + _offset, sizeof(uint8_t));
        _offset += sizeof(uint8_t);

        for (const uint64_t _metric : {_read, _write, _count})
        {
          const auto _metric_native = boost::endian::native_to_little(_metric);
          std::memcpy(write_buffer.data() + _offset, &_metric_native, sizeof(uint64_t));
          batch.emplace_back(write_buffer.data() + _offset, sizeof(uint64_t));
          _offset += sizeof(uint64_t);
        }
      }

      for (const auto &_channel : _frag)
      {
        std::memcpy(write_buffer.data() + _offset, &_channel, _channel.size());
        batch.emplace_back(write_buffer.data() + _offset, _channel.size());
        _offset += _channel.size();
      }
    }
  }
} // namespace throttr
