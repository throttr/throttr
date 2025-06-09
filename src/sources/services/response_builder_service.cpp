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
    write_buffer.resize(_offset + 1);
    write_buffer[_offset] = static_cast<std::byte>(entry->key_.size());
    batch->emplace_back(&write_buffer[_offset], 1);
    batch->emplace_back(&entry->entry_.type_, sizeof(entry->entry_.type_));
    batch->emplace_back(&entry->entry_.ttl_type_, sizeof(entry->entry_.ttl_type_));

    // Handling _expires_at
    {
      const auto _expires_at = entry->entry_.expires_at_.load(std::memory_order_acquire);

      const auto _off = write_buffer.size();
      append_uint64_t(write_buffer, native_to_little(_expires_at));
      batch->emplace_back(&write_buffer[_off], sizeof(_expires_at));
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
      batch->emplace_back(&write_buffer[_scoped_offset], sizeof(_bytes_used));
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
    batch->emplace_back(&write_buffer[_offset], 1);

#ifdef ENABLED_FEATURE_METRICS
    for (const auto &_metric = *entry->metrics_; uint64_t _v :
                                                 {_metric.reads_per_minute_.load(std::memory_order_relaxed),
                                                  _metric.writes_per_minute_.load(std::memory_order_relaxed),
                                                  _metric.reads_accumulator_.load(std::memory_order_relaxed),
                                                  _metric.writes_accumulator_.load(std::memory_order_relaxed)})
    {
      const auto _off = write_buffer.size();
      append_uint64_t(write_buffer, native_to_little(_v));
      batch->emplace_back(&write_buffer[_off], sizeof(_v));
    }
#else
    for (int _i = 0; _i < 4; ++_i)
    {
      constexpr uint64_t _empty = 0;
      const auto _off = write_buffer.size();
      append_uint64_t(write_buffer, native_to_little(_empty));
      batch->emplace_back(&write_buffer[_off], sizeof(_empty));
    }
#endif

    return 0;
  }

  void response_builder_service::handle_fragmented_connections_response(
    const std::shared_ptr<state> &state,
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::byte> &write_buffer)
  {
    batch.emplace_back(&state::success_response_, 1);

    using fragment_container =
      std::tuple<std::vector<const connection<tcp_socket> *>, std::vector<const connection<unix_socket> *>>;

    fragment_container _fragment;
    std::vector<fragment_container> _fragments;
    std::size_t _current_fragment_size = 0;
    constexpr std::size_t _max_fragment_size = 2048; // LCOV_EXCL_LINE

    auto _fragmenter = [&](auto &connections, auto &mutex)
    {
      std::lock_guard _lock(mutex);
      for (const auto &[_id, _conn] : connections)
      {
        const std::size_t _conn_size = write_connections_entry_to_buffer(state, nullptr, _conn, write_buffer, true);
        // LCOV_EXCL_START
        if (_current_fragment_size + _conn_size > _max_fragment_size)
        {
          _fragments.push_back(std::move(_fragment));
          std::get<0>(_fragment).clear();
          std::get<1>(_fragment).clear();
          _current_fragment_size = 0;
        }
        // LCOV_EXCL_STOP
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(_conn)>, connection<tcp_socket> *>)
          std::get<0>(_fragment).push_back(_conn);
        else
          std::get<1>(_fragment).push_back(_conn);
        _current_fragment_size += _conn_size;
      }
    };

    _fragmenter(state->tcp_connections_, state->tcp_connections_mutex_);
    _fragmenter(state->unix_connections_, state->unix_connections_mutex_);

    if (!std::get<0>(_fragment).empty() || !std::get<1>(_fragment).empty()) // LCOV_EXCL_LINE Note: Partially tested.
      _fragments.push_back(std::move(_fragment));

    push_total_fragments(batch, write_buffer, _fragments.size());

    std::size_t _index = 1;
    for (const auto &[_tcp_fragment, _unix_fragment] : _fragments)
    {
      const uint64_t _fragment_index = _index;
      ++_index;

      for (const auto _connection_count = _tcp_fragment.size() + _unix_fragment.size();
           const uint64_t _val : {_fragment_index, _connection_count})
      {
        const auto _offset = write_buffer.size();
        append_uint64_t(write_buffer, boost::endian::native_to_little(_val));
        batch.emplace_back(&write_buffer[_offset], sizeof(_val));
      }

      for (const auto *_conn : _tcp_fragment)
      {
        write_connections_entry_to_buffer<tcp_socket>(state, &batch, _conn, write_buffer, false);
      }
      for (const auto *_conn : _unix_fragment)
      {
        write_connections_entry_to_buffer<unix_socket>(state, &batch, _conn, write_buffer, false);
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

    batch.emplace_back(&state::success_response_, 1);

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
      batch.emplace_back(&write_buffer[_offset], sizeof(_fragment_count));
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
        batch.emplace_back(&write_buffer[_offset], sizeof(_value));
      }

      // Serialize entries
      for (const auto *_entry : _fragment) // LCOV_EXCL_LINE Note: Partially tested.
      {
        serialize_entry(&batch, _entry, false);
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
    batch.emplace_back(&state::success_response_, 1);

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
        batch.emplace_back(&write_buffer[_offset], sizeof(_val));
      }

      // Serialize channels in the fragment
      for (const auto &_chan : _frag)
      {
        const auto &[_read, _write, _count] = _channel_stats[_chan];
        const auto _size = static_cast<std::uint8_t>(_chan.size());

        const auto _offset = write_buffer.size();
        write_buffer.resize(_offset + 1);
        write_buffer[_offset] = std::byte{_size};
        batch.emplace_back(&write_buffer[_offset], 1);

        for (const uint64_t _metric : {_read, _write, _count})
        {
          const auto _scoped_offset = write_buffer.size();
          append_uint64_t(write_buffer, boost::endian::native_to_little(_metric));
          batch.emplace_back(&write_buffer[_scoped_offset], sizeof(_metric));
        }
      }

      for (const auto &_channel : _frag)
      {
        const auto _offset = write_buffer.size();
        write_buffer.resize(_offset + _channel.size());
        std::ranges::transform(_channel, write_buffer.begin() + _offset, [](char c) { return static_cast<std::byte>(c); });
        batch.emplace_back(&write_buffer[_offset], _channel.size());
      }
    }
  }
} // namespace throttr
