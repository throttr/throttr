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

class ChannelsTestFixture : public ServiceTestFixture
{
};

TEST_F(ChannelsTestFixture, OnSuccess)
{
  boost::asio::io_context _io_context;

  // Conexión 1: Suscribirse al canal "metrics"
  tcp::resolver _resolver1(_io_context);
  auto _endpoints1 = _resolver1.resolve("127.0.0.1", std::to_string(app_->state_->exposed_port_));
  tcp::socket _socket1(_io_context);
  boost::asio::connect(_socket1, _endpoints1);

  auto _subscribe_buffer = request_subscribe_builder("metrics");
  boost::asio::write(_socket1, boost::asio::buffer(_subscribe_buffer.data(), _subscribe_buffer.size()));

  std::vector<std::byte> _subscribe_response(1);
  boost::asio::read(_socket1, boost::asio::buffer(_subscribe_response.data(), _subscribe_response.size()));
  ASSERT_EQ(_subscribe_response[0], std::byte{0x01});

  // Conexión 2: Ejecutar comando CHANNELS
  tcp::resolver _resolver2(_io_context);
  auto _endpoints2 = _resolver2.resolve("127.0.0.1", std::to_string(app_->state_->exposed_port_));
  tcp::socket _socket2(_io_context);
  boost::asio::connect(_socket2, _endpoints2);

  // CHANNELS opcode = 0x16 (ejemplo)
  std::vector<std::byte> _channels_request = {std::byte{0x16}};
  boost::asio::write(_socket2, boost::asio::buffer(_channels_request));

  // Leer respuesta mínima: success (1 byte) + fragment count (8 bytes)
  std::vector<std::byte> _header_response(9);
  boost::asio::read(_socket2, boost::asio::buffer(_header_response));

  ASSERT_EQ(_header_response[0], std::byte{0x01});                                   // success
  const auto *_count_ptr = reinterpret_cast<const uint64_t *>(&_header_response[1]); // NOSONAR
  const uint64_t _fragment_count = *_count_ptr;
  ASSERT_GT(_fragment_count, 0);

  // Leer primer fragmento: index (8), count (8), [1-byte size, 8+8+8], luego nombre
  std::vector<std::byte> _fragment_header(16);
  boost::asio::read(_socket2, boost::asio::buffer(_fragment_header));

  const auto *_chan_count_ptr = reinterpret_cast<const uint64_t *>(&_fragment_header[8]); // NOSONAR
  ASSERT_GT(*_chan_count_ptr, 0);

  // Leer canal: 1 + 8 + 8 + 8 = 25 bytes por canal
  std::vector<std::byte> _channel_entry(25);
  boost::asio::read(_socket2, boost::asio::buffer(_channel_entry));

  const std::uint8_t _chan_size = std::to_integer<uint8_t>(_channel_entry[0]);
  ASSERT_EQ(_chan_size, 7); // "metrics".size() = 7

  // Leer nombre del canal
  std::vector<std::byte> _chan_name(_chan_size);
  boost::asio::read(_socket2, boost::asio::buffer(_chan_name));
  std::string _name(reinterpret_cast<const char *>(_chan_name.data()), _chan_name.size());

  ASSERT_EQ(_name, "metrics");
}