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

#include <throttr/connection.hpp>
#include <throttr/state.hpp>

namespace throttr
{
  void state::handle_connections(std::vector<boost::asio::const_buffer> &batch, std::vector<std::uint8_t> &write_buffer)
  {
#ifndef ENABLED_FEATURE_METRICS
    batch.emplace_back(boost::asio::buffer(&failed_response_, 1));
    return;
#endif
    handle_fragmented_connections_response(batch, write_buffer);
  }

  std::size_t state::write_connections_entry_to_buffer(
    std::vector<boost::asio::const_buffer> *batch,
    const connection *conn,
    std::vector<std::uint8_t> &write_buffer,
    const bool measure)
  {
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
    const auto & _ip_str = conn->ip_;
    const auto _len = std::min<std::size_t>(_ip_str.size(), 16);
    std::memcpy(_ip_bytes.data(), _ip_str.data(), _len);
    _push(_ip_bytes.data(), 16);

    // Port (2 bytes)
    _push(&conn->port_, sizeof(conn->port_));

#ifdef ENABLED_FEATURE_METRICS
    const auto &_metrics = *conn->metrics_;

    for (uint64_t val :
         {conn->connected_at_,
          _metrics.network_.read_bytes_.load(std::memory_order_relaxed),
          _metrics.network_.write_bytes_.load(std::memory_order_relaxed),
          _metrics.network_.published_bytes_.load(std::memory_order_relaxed),
          _metrics.network_.received_bytes_.load(std::memory_order_relaxed),
          _metrics.memory_.allocated_bytes_.load(std::memory_order_relaxed),
          _metrics.memory_.consumed_bytes_.load(std::memory_order_relaxed),
          _metrics.service_.insert_request_.load(std::memory_order_relaxed),
          _metrics.service_.set_request_.load(std::memory_order_relaxed),
          _metrics.service_.query_request_.load(std::memory_order_relaxed),
          _metrics.service_.get_request_.load(std::memory_order_relaxed),
          _metrics.service_.update_request_.load(std::memory_order_relaxed),
          _metrics.service_.purge_request_.load(std::memory_order_relaxed),
          _metrics.service_.list_request_.load(std::memory_order_relaxed),
          _metrics.service_.info_request_.load(std::memory_order_relaxed),
          _metrics.service_.stat_request_.load(std::memory_order_relaxed),
          _metrics.service_.stats_request_.load(std::memory_order_relaxed),
          _metrics.service_.subscribe_request_.load(std::memory_order_relaxed),
          _metrics.service_.unsubscribe_request_.load(std::memory_order_relaxed),
          _metrics.service_.connections_request_.load(std::memory_order_relaxed),
          _metrics.service_.connection_request_.load(std::memory_order_relaxed),
          _metrics.service_.channels_request_.load(std::memory_order_relaxed),
          _metrics.service_.channel_request_.load(std::memory_order_relaxed),
          _metrics.service_.whoami_request_.load(std::memory_order_relaxed)})
    {
      _push(&val, sizeof(val));
    }
#else
    // Rellenar con ceros si las métricas no están habilitadas
    std::array<std::uint8_t, 235 - 16 - 1 - 16 - 2> zeros = {};
    _push(zeros.data(), zeros.size());
#endif

    return 0;
  }

  void state::handle_fragmented_connections_response(
    std::vector<boost::asio::const_buffer> &batch,
    std::vector<std::uint8_t> &write_buffer)
  {
#ifndef ENABLED_FEATURE_METRICS
    batch.emplace_back(boost::asio::buffer(&failed_response_, 1));
    return;
#endif

    std::vector<const connection *> _fragment;
    std::vector<std::vector<const connection *>> _fragments;
    {
      std::size_t _current_fragment_size = 0;
      std::lock_guard _lock(connections_mutex_);
      for (const auto &[_id, _conn] : connections_)
      {
        const std::size_t _conn_size = write_connections_entry_to_buffer(nullptr, _conn, write_buffer, true);

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

    {
      const auto _offset = write_buffer.size();
      const uint64_t _total_fragments = _fragments.size();
      const auto *_ptr = reinterpret_cast<const std::uint8_t *>(&_total_fragments); // NOSONAR
      write_buffer.insert(write_buffer.end(), _ptr, _ptr + sizeof(_total_fragments));
      batch.emplace_back(boost::asio::buffer(&write_buffer[_offset], sizeof(_total_fragments)));
    }

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
        write_connections_entry_to_buffer(&batch, _conn, write_buffer, false);
      }
    }
  }

} // namespace throttr