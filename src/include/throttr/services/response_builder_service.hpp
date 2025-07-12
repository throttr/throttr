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
     * @param offset
     * @param measure
     * @return
     */
    static std::pair<std::size_t, std::size_t> write_list_entry_to_buffer(
      const std::shared_ptr<state> &state,
      std::vector<boost::asio::const_buffer> *batch,
      const entry_wrapper *entry,
      std::vector<std::byte> &write_buffer,
      std::size_t &offset,
      bool measure);

    /**
     * Write STATS entry to buffer
     *
     * @param state
     * @param batch
     * @param entry
     * @param write_buffer
     * @param offset
     * @param measure
     * @return
     */
    static std::pair<std::size_t, std::size_t> write_stats_entry_to_buffer(
      const std::shared_ptr<state> &state,
      std::vector<boost::asio::const_buffer> *batch,
      const entry_wrapper *entry,
      std::vector<std::byte> &write_buffer,
      std::size_t &offset,
      bool measure);

    /**
     * Write CONNECTIONS entry to buffer
     *
     * @param state
     * @param batch
     * @param conn
     * @param write_buffer
     * @param offset
     * @param measure
     * @return
     */
    template<typename Socket>
    static std::pair<std::size_t, std::size_t> write_connections_entry_to_buffer(
      const std::shared_ptr<state> &state,
      std::vector<boost::asio::const_buffer> *batch,
      const connection<Socket> *conn,
      std::vector<std::byte> &write_buffer,
      std::size_t &offset,
      const bool measure)
    {
      boost::ignore_unused(state);

      // 16 bytes uuid
      // 1 byte client or agent
      // 1 byte tcp or unix
      // 1 byte ip version
      // 16 bytes ip padded
      // 2 bytes port
      // 56 bytes io metrics
      // 144 bytes service metrics
      // 237 bytes

      if (measure)
        return {237, 1};

      using namespace boost::endian;

      const std::size_t _start_size = offset;

      // UUID (16 bytes)
      std::memcpy(write_buffer.data() + offset, conn->id_.data(), 16);
      offset += 16;

      // Type (client or agent)
      std::memcpy(write_buffer.data() + offset, &conn->type_, sizeof(uint8_t));
      offset += sizeof(uint8_t);

      // Kind (tcp or unix)
      std::memcpy(write_buffer.data() + offset, &conn->kind_, sizeof(uint8_t));
      offset += sizeof(uint8_t);

      // IP version (1 byte)
      const std::uint8_t _ip_version = conn->ip_.contains(':') ? 0x06 : 0x04;
      write_buffer[offset] = static_cast<std::byte>(_ip_version);
      offset += sizeof(uint8_t);

      // IP padded to 16 bytes
      std::array<std::uint8_t, 16> _ip_bytes = {};
      const auto &_ip_str = conn->ip_;
      const auto _len = std::min<std::size_t>(_ip_str.size(), 16);
      std::memcpy(_ip_bytes.data(), _ip_str.data(), _len);

      std::memcpy(write_buffer.data() + offset, &_ip_bytes, 16);
      offset += 16;

      // Port (2 bytes)
      const uint16_t _port = native_to_little(conn->port_);
      std::memcpy(write_buffer.data() + offset, &_port, sizeof(uint16_t));
      offset += sizeof(uint16_t);

      // there are 6 emplace back

#ifdef ENABLED_FEATURE_METRICS
      const auto &_metrics = *conn->metrics_;
#else
      constexpr uint64_t _empty = 0;
#endif

      for (const uint64_t _val :
           {native_to_little(conn->connected_at_),
#ifdef ENABLED_FEATURE_METRICS
            native_to_little(_metrics.network_.read_bytes_.accumulator_.load(std::memory_order_relaxed)),
            native_to_little(_metrics.network_.write_bytes_.accumulator_.load(std::memory_order_relaxed)),
            native_to_little(_metrics.network_.published_bytes_.accumulator_.load(std::memory_order_relaxed)),
            native_to_little(_metrics.network_.received_bytes_.accumulator_.load(std::memory_order_relaxed)),
            native_to_little(_metrics.memory_.allocated_bytes_.accumulator_.load(std::memory_order_relaxed)),
            native_to_little(_metrics.memory_.consumed_bytes_.accumulator_.load(std::memory_order_relaxed))})
#else
            _empty,
            _empty,
            _empty,
            _empty,
            _empty,
            _empty})
#endif
      { // 7 x 8 = 56 bytes on i/o metrics

        std::memcpy(write_buffer.data() + offset, &_val, sizeof(uint64_t));
        offset += sizeof(uint64_t);
      } // 7 emplace back

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
         request_types::whoami}; // 18 x 8 = 144 bytes on service metrics

      for (request_types type : monitored_request_types)
      {
        boost::ignore_unused(type);
#ifdef ENABLED_FEATURE_METRICS
        const auto &metric = conn->metrics_->commands_[static_cast<std::size_t>(type)];
        const uint64_t _value = native_to_little(metric.accumulator_.load(std::memory_order_relaxed));
#else
        constexpr uint64_t _value = 0;
#endif

        std::memcpy(write_buffer.data() + offset, &_value, sizeof(uint64_t));
        offset += sizeof(uint64_t);
      } // 18 emplace back

      batch->emplace_back(write_buffer.data() + _start_size, offset - _start_size);

      return {0, 0};
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
      const std::function<
        std::pair<std::size_t, std::size_t>(std::vector<boost::asio::const_buffer> *, const entry_wrapper *, std::size_t &, bool)>
        &serialize_entry);

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
