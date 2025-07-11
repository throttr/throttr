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

#include "../service_test_fixture.hpp"
#include <fmt/core.h>

class ChannelsTestFixture : public ServiceTestFixture
{
};

TEST_F(ChannelsTestFixture, OnSuccess)
{
  boost::asio::io_context _io_context;

  auto _subscriber = make_connection(_io_context);
  auto _socket = make_connection(_io_context);

  auto _subscribe_buffer = request_subscribe_builder("CHANNEL_ONE");
  boost::asio::write(_subscriber, boost::asio::buffer(_subscribe_buffer.data(), _subscribe_buffer.size()));

  std::vector<std::byte> _subscribe_response(1);
  boost::asio::read(_subscriber, boost::asio::buffer(_subscribe_response.data(), _subscribe_response.size()));
  ASSERT_EQ(_subscribe_response[0], std::byte{0x01});

  auto _channels_request = request_channels_builder();
  boost::asio::write(_socket, boost::asio::buffer(_channels_request));

  // Status
  std::vector<std::byte> _status_response(1);
  boost::asio::read(_socket, boost::asio::buffer(_status_response));
  ASSERT_EQ(_status_response[0], std::byte{0x01});

  // Fragments (P)
  std::vector<std::byte> _header_response(8);
  boost::asio::read(_socket, boost::asio::buffer(_header_response));
  const auto *_count_ptr = reinterpret_cast<const uint64_t *>(&_header_response[0]);
  const uint64_t _fragment_count = *_count_ptr;
  ASSERT_EQ(boost::endian::little_to_native(_fragment_count), 1);

  std::vector<std::byte> _fragment_header(16);
  boost::asio::read(_socket, boost::asio::buffer(_fragment_header));

  // Fragment
  const auto *_fragment_count_ptr = reinterpret_cast<const uint64_t *>(&_fragment_header[0]);
  ASSERT_EQ(boost::endian::little_to_native(*_fragment_count_ptr), 1);

  // Channels
  const auto *_channels_count_ptr = reinterpret_cast<const uint64_t *>(&_fragment_header[8]);
  ASSERT_EQ(boost::endian::little_to_native(*_channels_count_ptr), 4);
  auto _channel_count = boost::endian::little_to_native(*_channels_count_ptr);
  // Per Channel
  for (int _i = 0; _i < static_cast<int>(_channel_count); ++_i)
  {
    // Channel size
    std::vector<std::byte> _channel_size(1);
    boost::asio::read(_socket, boost::asio::buffer(_channel_size));

    // Read bytes
    std::vector<std::byte> _read_bytes(8);
    boost::asio::read(_socket, boost::asio::buffer(_read_bytes));
    uint64_t _read_bytes_number;
    std::memcpy(&_read_bytes_number, _read_bytes.data(), 8);
    ASSERT_GE(_read_bytes_number, 0);

    // Write bytes
    std::vector<std::byte> _write_bytes(8);
    boost::asio::read(_socket, boost::asio::buffer(_write_bytes));
    uint64_t _write_bytes_number;
    std::memcpy(&_write_bytes_number, _write_bytes.data(), 8);
    ASSERT_GE(_write_bytes_number, 0);

    // Subscribed connections
    std::vector<std::byte> _subscribed_connections(8);
    boost::asio::read(_socket, boost::asio::buffer(_subscribed_connections));
    uint64_t _subscribed_connections_number;
    std::memcpy(&_subscribed_connections_number, _subscribed_connections.data(), 8);
    ASSERT_GE(_subscribed_connections_number, 1);
  }

  boost::system::error_code _ec;
  _subscriber.close(_ec);
  _socket.close(_ec);
}
