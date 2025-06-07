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

class ConnectionsTestFixture : public ServiceTestFixture
{
};

TEST_F(ConnectionsTestFixture, OnSuccessSingleFragment)
{
  boost::asio::io_context _io_context;
  const auto _tcp_connection = make_tcp_connection(_io_context);
  const auto _unix_connection = make_connection(_io_context);
  const auto _conn_buffer = request_connections_builder();
  const auto _response = send_and_receive(
    _conn_buffer,
    1 +         // status
      8 +       // fragment count
      8 +       // fragment id
      8 +       // connection count
      (235 * 3) // una conexión con ENABLED_FEATURE_METRICS
  );

  size_t _offset = 1;

  // Fragment count
  uint64_t _fragment_count;
  std::memcpy(&_fragment_count, _response.data() + _offset, sizeof(_fragment_count));
  _offset += sizeof(_fragment_count);
  ASSERT_EQ(_fragment_count, 1);

  // Fragment ID
  uint64_t _fragment_id;
  std::memcpy(&_fragment_id, _response.data() + _offset, sizeof(_fragment_id));
  _offset += sizeof(_fragment_id);
  ASSERT_EQ(_fragment_id, 1);

  // Connection count
  uint64_t _connection_count;
  std::memcpy(&_connection_count, _response.data() + _offset, sizeof(_connection_count));
  _offset += sizeof(_connection_count);
  ASSERT_GE(_connection_count, 3);

  ASSERT_LE(_offset + 227, _response.size());

  // UUID (16 bytes)
  _offset += 16;

  // IP version (1 byte)
  const uint8_t _ip_version = std::to_integer<uint8_t>(_response[_offset]);
  ASSERT_TRUE(_ip_version == 0x04 || _ip_version == 0x06);
  _offset += 1;

  // IP padded to 16 bytes
  _offset += 16;

  // Port (2 bytes)
  uint16_t _port;
  std::memcpy(&_port, _response.data() + _offset, sizeof(_port));
  _offset += sizeof(_port);
  _port = boost::endian::native_to_little(_port);
  ASSERT_GT(_port, 0);

  // 24 métricas (8 bytes cada una)
  for (int i = 0; i < 25; ++i)
  {
    uint64_t _metric;
    std::memcpy(&_metric, _response.data() + _offset, sizeof(_metric));
    _offset += sizeof(_metric);
  }

  ASSERT_EQ(_offset, 1 + 8 + 8 + 8 + 235);
  ASSERT_LE(_offset, _response.size());
}
