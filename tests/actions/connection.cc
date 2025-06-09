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
protected:
  template<typename Transport> void test_on_success(Transport &socket)
  {
    auto _uuid = get_connection_id(socket);

    // CONNECTION
    std::array<std::byte, 16> _usable_uuid;
    for (std::size_t i = 0; i < 16; ++i)
      _usable_uuid[i] = static_cast<std::byte>(_uuid.data[i]);

    const auto _conn_buffer = request_connection_builder(_usable_uuid);
    boost::asio::write(socket, boost::asio::buffer(_conn_buffer.data(), _conn_buffer.size()));

    std::vector<std::byte> _status_segment(1);
    boost::asio::read(socket, boost::asio::buffer(_status_segment.data(), _status_segment.size()));
    ASSERT_EQ(_status_segment[0], std::byte{0x01});

    std::vector<std::byte> _uuid_segment(16);
    boost::asio::read(socket, boost::asio::buffer(_uuid_segment.data(), _uuid_segment.size()));
    boost::uuids::uuid _returned_uuid;
    std::memcpy(_returned_uuid.data, _uuid_segment.data(), 16);
    ASSERT_EQ(_returned_uuid, _uuid);

    std::vector<std::byte> _type_segment(1);
    boost::asio::read(socket, boost::asio::buffer(_type_segment.data(), _type_segment.size()));
    ASSERT_EQ(_type_segment[0], static_cast<std::byte>(connection_type::client));

    std::vector<std::byte> _kind_segment(1);
    boost::asio::read(socket, boost::asio::buffer(_kind_segment.data(), _kind_segment.size()));
    ASSERT_TRUE(
      _kind_segment[0] == static_cast<std::byte>(connection_kind::unix_socket) ||
      _kind_segment[0] == static_cast<std::byte>(connection_kind::tcp_socket));

    std::vector<std::byte> _ip_version_segment(1);
    boost::asio::read(socket, boost::asio::buffer(_ip_version_segment.data(), _ip_version_segment.size()));
    const uint8_t _ip_version = std::to_integer<uint8_t>(_ip_version_segment[0]);
    ASSERT_TRUE(_ip_version == 0x04 || _ip_version == 0x06);

    std::vector<std::byte> _ip_segment(16);
    boost::asio::read(socket, boost::asio::buffer(_ip_segment.data(), _ip_segment.size()));

    std::vector<std::byte> _port_segment(2);
    boost::asio::read(socket, boost::asio::buffer(_port_segment.data(), _port_segment.size()));
    uint16_t _port;
    std::memcpy(&_port, _port_segment.data(), sizeof(_port));
    _port = boost::endian::little_to_native(_port);
    ASSERT_GT(_port, 0);

    for (int i = 0; i < 25; ++i)
    {
      uint64_t _metric;
      std::vector<std::byte> _metric_segment(8);
      boost::asio::read(socket, boost::asio::buffer(_metric_segment.data(), _metric_segment.size()));
      std::memcpy(&_metric, _metric_segment.data(), sizeof(_metric));
      ASSERT_GE(_metric, 0);
    }

    boost::system::error_code _ec;
    socket.close(_ec);
  }
};

TEST_F(ConnectionTestFixture, OnSuccess)
{
  boost::asio::io_context _io_context;
  auto _unix_socket = make_connection(_io_context);
  test_on_success(_unix_socket);
  auto _tcp_socket = make_tcp_connection(_io_context);
  test_on_success(_tcp_socket);
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
