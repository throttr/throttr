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
  auto _socket1 = make_connection(_io_context);
  auto _socket2 = make_connection(_io_context);

  auto _subscribe_buffer = request_subscribe_builder("metrics");
  boost::asio::write(_socket1, boost::asio::buffer(_subscribe_buffer.data(), _subscribe_buffer.size()));

  std::vector<std::byte> _subscribe_response(1);
  boost::asio::read(_socket1, boost::asio::buffer(_subscribe_response.data(), _subscribe_response.size()));
  ASSERT_EQ(_subscribe_response[0], std::byte{0x01});

  // 🔌 Conexión 2: CHANNEL request
  std::string _chan = "metrics";
  std::vector<std::byte> _channel_request = {std::byte{0x17}, std::byte{static_cast<std::uint8_t>(_chan.size())}};
  _channel_request.insert(
    _channel_request.end(),
    reinterpret_cast<const std::byte *>(_chan.data()),
    reinterpret_cast<const std::byte *>(_chan.data() + _chan.size()));

  boost::asio::write(_socket2, boost::asio::buffer(_channel_request.data(), _channel_request.size()));

  std::vector<std::byte> _header(1);
  boost::asio::read(_socket2, boost::asio::buffer(_header));
  ASSERT_EQ(_header[0], std::byte{0x01});

  std::vector<std::byte> _count_buf(8);
  boost::asio::read(_socket2, boost::asio::buffer(_count_buf));
  const auto *_count_ptr = reinterpret_cast<const uint64_t *>(_count_buf.data()); // NOSONAR
  ASSERT_EQ(boost::endian::little_to_native(*_count_ptr), 1);

  std::vector<std::byte> _data(40);
  boost::asio::read(_socket2, boost::asio::buffer(_data));

  ASSERT_EQ(_data.size(), 40);

  boost::system::error_code _ec;
  _socket1.close(_ec);
  _socket2.close(_ec);
}

TEST_F(ChannelTestFixture, OnFailed)
{
  boost::asio::io_context _io_context;

  // 🔌 Conexión directa sin suscribirse a nada
  auto _socket = make_connection(_io_context);

  // Enviar CHANNEL request a un canal inexistente
  std::string _chan = "nope-channel";
  std::vector<std::byte> _request = {std::byte{0x17}, std::byte{static_cast<std::uint8_t>(_chan.size())}};
  _request.insert(
    _request.end(),
    reinterpret_cast<const std::byte *>(_chan.data()),
    reinterpret_cast<const std::byte *>(_chan.data() + _chan.size()));

  boost::asio::write(_socket, boost::asio::buffer(_request));

  // Leer 1 byte de respuesta
  std::vector<std::byte> _response(1);
  boost::asio::read(_socket, boost::asio::buffer(_response));

  // Debe ser 0x00 → canal no existe
  ASSERT_EQ(_response[0], std::byte{0x00});

  boost::system::error_code _ec;
  _socket.close(_ec);
}
