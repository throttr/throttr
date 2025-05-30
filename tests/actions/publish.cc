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
  boost::asio::io_context _io_context;
  tcp::resolver _resolver(_io_context);
  const auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(app_->state_->exposed_port_));

  // Socket 1: el que se suscribe
  tcp::socket _subscriber(_io_context);
  boost::asio::connect(_subscriber, _endpoints);

  // Socket 2: el que publica
  tcp::socket _publisher(_io_context);
  boost::asio::connect(_publisher, _endpoints);

  // SUBSCRIBE desde socket 1
  const auto _subscribe_buffer = request_subscribe_builder("news");
  boost::asio::write(_subscriber, boost::asio::buffer(_subscribe_buffer.data(), _subscribe_buffer.size()));

  std::vector<std::byte> _subscribe_response(1);
  boost::asio::read(_subscriber, boost::asio::buffer(_subscribe_response.data(), _subscribe_response.size()));

  ASSERT_EQ(_subscribe_response[0], std::byte{0x01});

  // PUBLISH desde socket 2
  const auto _publish_buffer = request_publish_builder({std::byte{0x42}}, "news");
  boost::asio::write(_publisher, boost::asio::buffer(_publish_buffer.data(), _publish_buffer.size()));

  // Leer tipo
  std::vector<std::byte> _header(1 + sizeof(value_type));
  boost::asio::read(_subscriber, boost::asio::buffer(_header.data(), _header.size()));

  ASSERT_EQ(_header[0], std::byte{0x03});

  value_type _size = 0;
  std::memcpy(&_size, _header.data() + 1, sizeof(value_type));

  std::vector<std::byte> _payload(_size);
  boost::asio::read(_subscriber, boost::asio::buffer(_payload.data(), _payload.size()));

  ASSERT_EQ(_size, 1);
  ASSERT_EQ(_payload[0], std::byte{0x42});
}