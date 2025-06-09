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

#ifndef THROTTR_SERVICES_RESPONSE_BUILDER_HPP
#define THROTTR_SERVICES_RESPONSE_BUILDER_HPP

#include <boost/endian/conversion.hpp>
#include <functional>
#include <memory>

#include <boost/asio/buffer.hpp>
#include <throttr/connection.hpp>
#include <throttr/protocol_wrapper.hpp>

namespace throttr
{
  /**
   * Forward state
   */
  class state;

  /**
   * Forward connection
   */
  struct entry_wrapper;

  /**
   * Update service
   */
  class response_builder_service
  {
  public:
    /**
     * Write LIST entry to buffer
     *
     * @param state
     * @param batch
     * @param entry
     * @param write_buffer
     * @param measure
     * @return
     */
    static std::size_t write_list_entry_to_buffer(
      const std::shared_ptr<state> &state,
      std::vector<boost::asio::const_buffer> *batch,
      const entry_wrapper *entry,
      std::vector<std::byte> &write_buffer,
      bool measure);

    /**
     * Write STATS entry to buffer
     *
     * @param state
     * @param batch
     * @param entry
     * @param write_buffer
     * @param measure
     * @return
     */
    static std::size_t write_stats_entry_to_buffer(
      const std::shared_ptr<state> &state,
      std::vector<boost::asio::const_buffer> *batch,
      const entry_wrapper *entry,
      std::vector<std::byte> &write_buffer,
      bool measure);

    /**
     * Write CONNECTIONS entry to buffer
     *
     * @param state
     * @param batch
     * @param conn
     * @param write_buffer
     * @param measure
     * @return
     */
    template<typename Socket>
    static std::size_t write_connections_entry_to_buffer(
      const std::shared_ptr<state> &state,
      std::vector<boost::asio::const_buffer> *batch,
      const connection<Socket> *conn,
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
        batch->emplace_back(&write_buffer[_offset], size); // NOSONAR
      };

      // UUID (16 bytes)
      _push(conn->id_.data(), 16);

      // Type (client or agent)
      const auto _type = static_cast<std::uint8_t>(conn->type_);
      _push(&_type, 1);

      // Kind (tcp or unix)
      const auto _kind = static_cast<std::uint8_t>(conn->kind_);
      _push(&_kind, 1);

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
        batch->emplace_back(&write_buffer[_offset], sizeof(uint64_t));
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
        batch->emplace_back(&write_buffer[_offset], sizeof(uint64_t));
      }

      return 0;
    }

    /**
     * Handle fragmented connections response
     *
     * @param state
     * @param batch
     * @param write_buffer
     */
    void handle_fragmented_connections_response(
      const std::shared_ptr<state> &state,
      std::vector<boost::asio::const_buffer> &batch,
      std::vector<std::byte> &write_buffer);

    /**
     * Handle fragmented entries response
     *
     * @param state
     * @param batch
     * @param write_buffer
     * @param max_fragment_size
     * @param serialize_entry
     */
    static void handle_fragmented_entries_response(
      const std::shared_ptr<state> &state,
      std::vector<boost::asio::const_buffer> &batch,
      std::vector<std::byte> &write_buffer,
      const std::size_t max_fragment_size,
      const std::function<std::size_t(std::vector<boost::asio::const_buffer> *, const entry_wrapper *, bool)> &serialize_entry);

    /**
     * Handle fragmented channels response
     *
     * @param state
     * @param batch
     * @param write_buffer
     */
    static void handle_fragmented_channels_response(
      const std::shared_ptr<state> &state,
      std::vector<boost::asio::const_buffer> &batch,
      std::vector<std::byte> &write_buffer);
  };
} // namespace throttr

#endif // THROTTR_SERVICES_RESPONSE_BUILDER_HPP
