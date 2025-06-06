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
    static std::size_t write_connections_entry_to_buffer(
      const std::shared_ptr<state> &state,
      std::vector<boost::asio::const_buffer> *batch,
      const connection<local_transport_socket> *conn,
      std::vector<std::byte> &write_buffer,
      bool measure);

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
