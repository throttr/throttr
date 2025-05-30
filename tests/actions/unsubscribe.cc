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

class UnsubscribeTestFixture : public ServiceTestFixture
{
};

TEST_F(UnsubscribeTestFixture, OnSuccess)
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

  // UNSUBSCRIBE
  const auto _unsubscribe_buffer = request_unsubscribe_builder("metrics");
  boost::asio::write(_socket, boost::asio::buffer(_unsubscribe_buffer.data(), _unsubscribe_buffer.size()));

  std::vector<std::byte> _unsubscribe_response(1);
  boost::asio::read(_socket, boost::asio::buffer(_unsubscribe_response.data(), _unsubscribe_response.size()));
  ASSERT_EQ(_unsubscribe_response[0], std::byte{0x01});
}

TEST_F(UnsubscribeTestFixture, OnFailed)
{
  boost::asio::io_context _io_context;
  tcp::resolver _resolver(_io_context);
  const auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(app_->state_->exposed_port_));

  tcp::socket _socket(_io_context);
  boost::asio::connect(_socket, _endpoints);

  // UNSUBSCRIBE without SUBSCRIBE
  const auto _unsubscribe_buffer = request_unsubscribe_builder("metrics");
  boost::asio::write(_socket, boost::asio::buffer(_unsubscribe_buffer.data(), _unsubscribe_buffer.size()));

  std::vector<std::byte> _unsubscribe_response(1);
  boost::asio::read(_socket, boost::asio::buffer(_unsubscribe_response.data(), _unsubscribe_response.size()));
  ASSERT_EQ(_unsubscribe_response[0], std::byte{0x00});
}