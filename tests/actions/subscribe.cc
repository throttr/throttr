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

class SubscribeTestFixture : public ServiceTestFixture
{
};

TEST_F(SubscribeTestFixture, OnSuccess)
{
  boost::asio::io_context _io_context;
  tcp::resolver _resolver(_io_context);
  const auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(app_->state_->exposed_port_));

  tcp::socket _socket(_io_context);
  boost::asio::connect(_socket, _endpoints);

  // SUBSCRIBE
  const auto _subscribe_buffer = request_subscribe_builder("metrics");
  boost::asio::write(_socket, boost::asio::buffer(_subscribe_buffer.data(), _subscribe_buffer.size()));

  std::vector<std::byte> _subscribe_response(1);
  boost::asio::read(_socket, boost::asio::buffer(_subscribe_response.data(), _subscribe_response.size()));

  ASSERT_EQ(_subscribe_response[0], std::byte{0x01});

  boost::system::error_code _ec;
  _socket.close(_ec);
}

TEST_F(SubscribeTestFixture, OnFailed)
{
  boost::asio::io_context _io_context;
  tcp::resolver _resolver(_io_context);
  const auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(app_->state_->exposed_port_));

  tcp::socket _socket(_io_context);
  boost::asio::connect(_socket, _endpoints);

  // SUBSCRIBE 1
  const auto _subscribe_buffer_1 = request_subscribe_builder("metrics");
  boost::asio::write(_socket, boost::asio::buffer(_subscribe_buffer_1.data(), _subscribe_buffer_1.size()));

  std::vector<std::byte> _subscribe_response_1(1);
  boost::asio::read(_socket, boost::asio::buffer(_subscribe_response_1.data(), _subscribe_response_1.size()));

  ASSERT_EQ(_subscribe_response_1[0], std::byte{0x01});

  // // SUBSCRIBE 2 (same)
  // const auto _subscribe_buffer_2 = request_subscribe_builder("metrics");
  // boost::asio::write(_socket, boost::asio::buffer(_subscribe_buffer_2.data(), _subscribe_buffer_2.size()));
  //
  // std::vector<std::byte> _subscribe_response_2(1);
  // boost::asio::read(_socket, boost::asio::buffer(_subscribe_response_2.data(), _subscribe_response_2.size()));
  //
  // ASSERT_EQ(_subscribe_response_2[0], std::byte{0x00});

  boost::system::error_code _ec;
  _socket.close(_ec);
}