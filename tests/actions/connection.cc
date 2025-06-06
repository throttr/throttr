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
#include <boost/endian/conversion.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <fmt/core.h>

class ConnectionTestFixture : public ServiceTestFixture
{
};

TEST_F(ConnectionTestFixture, OnSuccessUnix)
{
  boost::asio::io_context _io_context;
  auto _socket = make_connection(_io_context);

  // WHOAMI
  const auto _whoami_buffer = request_whoami_builder();
  boost::asio::write(_socket, boost::asio::buffer(_whoami_buffer.data(), _whoami_buffer.size()));

  std::vector<std::byte> _whoami_response(1 + 16);
  boost::asio::read(_socket, boost::asio::buffer(_whoami_response.data(), _whoami_response.size()));

  ASSERT_EQ(_whoami_response[0], std::byte{0x01});
  boost::uuids::uuid _uuid;
  std::memcpy(_uuid.data, _whoami_response.data() + 1, 16);

  // CONNECTION
  std::array<std::byte, 16> _usable_uuid;
  for (std::size_t i = 0; i < 16; ++i)
    _usable_uuid[i] = static_cast<std::byte>(_uuid.data[i]);
  const auto _conn_buffer = request_connection_builder(_usable_uuid);
  boost::asio::write(_socket, boost::asio::buffer(_conn_buffer.data(), _conn_buffer.size()));

  std::vector<std::byte> _conn_response(1 + 235);
  boost::asio::read(_socket, boost::asio::buffer(_conn_response.data(), _conn_response.size()));

  size_t _offset = 0;
  ASSERT_EQ(_conn_response[_offset], std::byte{0x01});
  _offset += 1;

  boost::uuids::uuid _returned_uuid;
  std::memcpy(_returned_uuid.data, _conn_response.data() + _offset, 16);
  _offset += 16;
  ASSERT_EQ(_returned_uuid, _uuid);

  const uint8_t _ip_version = std::to_integer<uint8_t>(_conn_response[_offset]);
  ASSERT_TRUE(_ip_version == 0x04 || _ip_version == 0x06);
  _offset += 1;

  _offset += 16; // IP padded
  uint16_t _port;
  std::memcpy(&_port, _conn_response.data() + _offset, sizeof(_port));
  _offset += sizeof(_port);
  _port = boost::endian::little_to_native(_port);
  ASSERT_GT(_port, 0);

  for (int i = 0; i < 25; ++i)
  {
    uint64_t _metric;
    std::memcpy(&_metric, _conn_response.data() + _offset, sizeof(_metric));
    _offset += sizeof(_metric);
  }

  ASSERT_EQ(_offset, 236);

  boost::system::error_code _ec;
  _socket.close(_ec);
}

TEST_F(ConnectionTestFixture, OnSuccessTCP)
{
  boost::asio::io_context _io_context;
  auto _socket = make_tcp_connection(_io_context);

  // WHOAMI
  const auto _whoami_buffer = request_whoami_builder();
  boost::asio::write(_socket, boost::asio::buffer(_whoami_buffer.data(), _whoami_buffer.size()));

  std::vector<std::byte> _whoami_response(1 + 16);
  boost::asio::read(_socket, boost::asio::buffer(_whoami_response.data(), _whoami_response.size()));

  ASSERT_EQ(_whoami_response[0], std::byte{0x01});
  boost::uuids::uuid _uuid;
  std::memcpy(_uuid.data, _whoami_response.data() + 1, 16);

  // CONNECTION
  std::array<std::byte, 16> _usable_uuid;
  for (std::size_t i = 0; i < 16; ++i)
    _usable_uuid[i] = static_cast<std::byte>(_uuid.data[i]);
  const auto _conn_buffer = request_connection_builder(_usable_uuid);
  boost::asio::write(_socket, boost::asio::buffer(_conn_buffer.data(), _conn_buffer.size()));

  std::vector<std::byte> _conn_response(1 + 235);
  boost::asio::read(_socket, boost::asio::buffer(_conn_response.data(), _conn_response.size()));

  size_t _offset = 0;
  ASSERT_EQ(_conn_response[_offset], std::byte{0x01});
  _offset += 1;

  boost::uuids::uuid _returned_uuid;
  std::memcpy(_returned_uuid.data, _conn_response.data() + _offset, 16);
  _offset += 16;
  ASSERT_EQ(_returned_uuid, _uuid);

  const uint8_t _ip_version = std::to_integer<uint8_t>(_conn_response[_offset]);
  ASSERT_TRUE(_ip_version == 0x04 || _ip_version == 0x06);
  _offset += 1;

  _offset += 16; // IP padded
  uint16_t _port;
  std::memcpy(&_port, _conn_response.data() + _offset, sizeof(_port));
  _offset += sizeof(_port);
  _port = boost::endian::little_to_native(_port);
  ASSERT_GT(_port, 0);

  for (int i = 0; i < 25; ++i)
  {
    uint64_t _metric;
    std::memcpy(&_metric, _conn_response.data() + _offset, sizeof(_metric));
    _offset += sizeof(_metric);
  }

  ASSERT_EQ(_offset, 236);

  boost::system::error_code _ec;
  _socket.close(_ec);
}

TEST_F(ConnectionTestFixture, OnFailed)
{
  boost::asio::io_context _io_context;
  auto _socket = make_connection(_io_context);

  const auto _fake_uuid = boost::uuids::random_generator()();
  std::array<std::byte, 16> _uuid;
  for (std::size_t i = 0; i < 16; ++i)
    _uuid[i] = static_cast<std::byte>(_fake_uuid.data[i]);

  const auto _conn_buffer = request_connection_builder(_uuid);
  boost::asio::write(_socket, boost::asio::buffer(_conn_buffer.data(), _conn_buffer.size()));

  std::vector<std::byte> _response(1);
  boost::asio::read(_socket, boost::asio::buffer(_response.data(), _response.size()));

  ASSERT_EQ(_response[0], std::byte{0x00});

  boost::system::error_code _ec;
  _socket.close(_ec);
}
