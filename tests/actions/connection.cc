#include "../service_test_fixture.hpp"
#include <boost/uuid/uuid_io.hpp>
#include <fmt/core.h>

class ConnectionTestFixture : public ServiceTestFixture
{
};

TEST_F(ConnectionTestFixture, OnSuccess)
{
  boost::asio::io_context _io_context;
  tcp::resolver _resolver(_io_context);
  const auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(app_->state_->exposed_port_));

  tcp::socket _socket(_io_context);
  boost::asio::connect(_socket, _endpoints);

  // WHOAMI
  const auto _whoami_buffer = request_whoami_builder();
  boost::asio::write(_socket, boost::asio::buffer(_whoami_buffer.data(), _whoami_buffer.size()));

  std::vector<std::byte> _whoami_response(1 + 16);
  boost::asio::read(_socket, boost::asio::buffer(_whoami_response.data(), _whoami_response.size()));

  ASSERT_EQ(_whoami_response[0], std::byte{0x01});
  boost::uuids::uuid _uuid;
  std::memcpy(_uuid.data, _whoami_response.data() + 1, 16);

  // CONNECTION
  const auto _conn_buffer = request_connection_builder(_uuid);
  boost::asio::write(_socket, boost::asio::buffer(_conn_buffer.data(), _conn_buffer.size()));

  std::vector<std::byte> _conn_response(1 + 227);
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
  ASSERT_GT(_port, 0);

  for (int i = 0; i < 24; ++i)
  {
    uint64_t _metric;
    std::memcpy(&_metric, _conn_response.data() + _offset, sizeof(_metric));
    _offset += sizeof(_metric);
  }

  ASSERT_EQ(_offset, 228);
}
