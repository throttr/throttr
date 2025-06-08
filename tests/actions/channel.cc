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
#include <boost/uuid/uuid_io.hpp>
#include <fmt/core.h>

class ChannelTestFixture : public ServiceTestFixture
{
};

TEST_F(ChannelTestFixture, OnSuccess)
{
  boost::asio::io_context _io_context;
  uint64_t _timestamp = std::chrono::system_clock::now().time_since_epoch().count();
  auto _subscriber = make_connection(_io_context);
  auto _subscriber_id = get_connection_id(_subscriber);
  auto _socket = make_connection(_io_context);

  auto _subscribe_buffer = request_subscribe_builder("CHANNEL_TEST");
  boost::asio::write(_subscriber, boost::asio::buffer(_subscribe_buffer.data(), _subscribe_buffer.size()));

  std::vector<std::byte> _subscribe_response(1);
  boost::asio::read(_subscriber, boost::asio::buffer(_subscribe_response.data(), _subscribe_response.size()));
  ASSERT_EQ(_subscribe_response[0], std::byte{0x01});

  auto _channel_buffer = request_channel_builder("CHANNEL_TEST");
  boost::asio::write(_socket, boost::asio::buffer(_channel_buffer.data(), _channel_buffer.size()));

  std::vector<std::byte> _header(1);
  boost::asio::read(_socket, boost::asio::buffer(_header));

  // Success
  ASSERT_EQ(_header[0], std::byte{0x01});

  // Numbers of subscribers (Q)
  std::vector<std::byte> _count_buf(8);
  boost::asio::read(_socket, boost::asio::buffer(_count_buf));
  const auto *_count_ptr = reinterpret_cast<const uint64_t *>(_count_buf.data()); // NOSONAR
  ASSERT_EQ(boost::endian::little_to_native(*_count_ptr), 1);

  // Connection ID
  std::vector<std::byte> _id_data(16);
  boost::asio::read(_socket, boost::asio::buffer(_id_data));
  boost::uuids::uuid _uuid{};
  std::memcpy(_uuid.data, _id_data.data(), 16);
  ASSERT_TRUE(_uuid == _subscriber_id);

  // Subscribed At
  std::vector<std::byte> _subscribed_at(8);
  boost::asio::read(_socket, boost::asio::buffer(_subscribed_at));
  uint64_t _subscribed_at_number;
  std::memcpy(&_subscribed_at_number, _subscribed_at.data(), 8);
  ASSERT_GE(_subscribed_at_number, _timestamp);

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

  boost::system::error_code _ec;
  _subscriber.close(_ec);
  _socket.close(_ec);
}

TEST_F(ChannelTestFixture, OnFailed)
{
  boost::asio::io_context _io_context;
  auto _socket = make_connection(_io_context);

  auto _channel_buffer = request_channel_builder("MISSING_CHANNEL");
  boost::asio::write(_socket, boost::asio::buffer(_channel_buffer));

  // Status as failed
  std::vector<std::byte> _response(1);
  boost::asio::read(_socket, boost::asio::buffer(_response));
  ASSERT_EQ(_response[0], std::byte{0x00});

  boost::system::error_code _ec;
  _socket.close(_ec);
}
