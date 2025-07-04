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

class InfoTestFixture : public ServiceTestFixture
{
};

TEST_F(InfoTestFixture, OnSuccess)
{
  boost::asio::io_context _io_context;
  auto _socket = make_connection(_io_context);

  // SUBSCRIBE #1
  const auto _sub_1 = request_subscribe_builder("metrics");
  boost::asio::write(_socket, boost::asio::buffer(_sub_1.data(), _sub_1.size()));
  std::vector<std::byte> _res_1(1);
  boost::asio::read(_socket, boost::asio::buffer(_res_1));
  ASSERT_EQ(_res_1[0], std::byte{0x01});

  // SUBSCRIBE #2
  const auto _sub_2 = request_subscribe_builder("connection");
  boost::asio::write(_socket, boost::asio::buffer(_sub_2.data(), _sub_2.size()));
  std::vector<std::byte> _res_2(1);
  boost::asio::read(_socket, boost::asio::buffer(_res_2));
  ASSERT_EQ(_res_2[0], std::byte{0x01});

  // INSERT contador
  const auto _insert = request_insert_builder(5, ttl_types::seconds, 60, "consumer/insert");
  boost::asio::write(_socket, boost::asio::buffer(_insert.data(), _insert.size()));
  std::vector<std::byte> _res_insert(1);
  boost::asio::read(_socket, boost::asio::buffer(_res_insert));
  ASSERT_EQ(_res_insert[0], std::byte{0x01});

  // SET buffer
  const std::string _key = "consumer/set";
  const std::vector _value = {std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF}};
  const auto _set = request_set_builder(_value, ttl_types::seconds, 60, _key);
  boost::asio::write(_socket, boost::asio::buffer(_set.data(), _set.size()));
  std::vector<std::byte> _res_set(1);
  boost::asio::read(_socket, boost::asio::buffer(_res_set));
  ASSERT_EQ(_res_set[0], std::byte{0x01});

  std::this_thread::sleep_for(std::chrono::seconds(1));

  // INFO
  const auto _info = request_info_builder();
  boost::asio::write(_socket, boost::asio::buffer(_info.data(), _info.size()));

  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::vector<std::byte> _response(433);
  boost::asio::read(_socket, boost::asio::buffer(_response.data(), _response.size()));

  ASSERT_EQ(_response[0], std::byte{0x01});
  ASSERT_EQ(_response.size(), 433);
  boost::system::error_code _ec;
  _socket.close(_ec);
}
