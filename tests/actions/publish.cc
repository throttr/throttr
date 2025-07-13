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

class PublishTestFixture : public ServiceTestFixture
{
};

TEST_F(PublishTestFixture, OnSuccess)
{
  boost::asio::io_context _subscriber_io_context;
  boost::asio::io_context _tcp_subscriber_io_context;
  boost::asio::io_context _publisher_io_context;
  auto _subscriber = make_connection(_subscriber_io_context);
  auto _tcp_subscriber = make_connection(_tcp_subscriber_io_context);
  auto _publisher = make_connection(_publisher_io_context);

  const auto _subscribe_buffer = request_subscribe_builder("news");

  boost::asio::write(_subscriber, boost::asio::buffer(_subscribe_buffer.data(), _subscribe_buffer.size()));
  boost::asio::write(_tcp_subscriber, boost::asio::buffer(_subscribe_buffer.data(), _subscribe_buffer.size()));
  boost::asio::write(_publisher, boost::asio::buffer(_subscribe_buffer.data(), _subscribe_buffer.size()));

  std::vector<std::byte> _subscribe_response(1);
  boost::asio::read(_subscriber, boost::asio::buffer(_subscribe_response.data(), _subscribe_response.size()));

  std::vector<std::byte> _tcp_subscribe_response(1);
  boost::asio::read(_tcp_subscriber, boost::asio::buffer(_tcp_subscribe_response.data(), _tcp_subscribe_response.size()));

  std::vector<std::byte> _publisher_subscribe_response(1);
  boost::asio::read(_publisher, boost::asio::buffer(_publisher_subscribe_response.data(), _publisher_subscribe_response.size()));

  ASSERT_EQ(_subscribe_response[0], std::byte{0x01});
  ASSERT_EQ(_tcp_subscribe_response[0], std::byte{0x01});
  ASSERT_EQ(_publisher_subscribe_response[0], std::byte{0x01});

  const auto _publish_buffer = request_publish_builder({std::byte{0x42}}, "news");
  boost::asio::write(_publisher, boost::asio::buffer(_publish_buffer.data(), _publish_buffer.size()));

  std::vector<std::byte> _header(2 + sizeof(value_type));
  boost::asio::read(_subscriber, boost::asio::buffer(_header.data(), _header.size()));

  ASSERT_EQ(_header[0], std::byte{static_cast<std::uint8_t>(request_types::event)});

  uint8_t _channel_size = 0;
  std::memcpy(&_channel_size, _header.data() + 1, sizeof(uint8_t));
  _channel_size = boost::endian::little_to_native(_channel_size);

  value_type _payload_size = 0;
  std::memcpy(&_payload_size, _header.data() + 2, sizeof(value_type));
  _payload_size = boost::endian::little_to_native(_payload_size);

  std::vector<std::byte> _payload(_payload_size + _channel_size);
  boost::asio::read(_subscriber, boost::asio::buffer(_payload.data(), _payload.size()));

  ASSERT_EQ(_payload_size, 1);
  ASSERT_EQ(_payload[_channel_size + 0], std::byte{0x42});

  boost::system::error_code _ec;
  _subscriber.close(_ec);
  _publisher.close(_ec);
  _tcp_subscriber.close(_ec);
}