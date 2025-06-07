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

class StatTestFixture : public ServiceTestFixture
{
};

TEST_F(StatTestFixture, OnSuccess)
{
  boost::asio::io_context _io_context;
  auto _socket = make_connection(_io_context);
  auto _tcp_socket = make_tcp_connection(_io_context);
  const std::string _key = "consumer/stat_test";

  const auto _insert = request_insert_builder(100, ttl_types::seconds, 120, _key);
  auto _insert_response = send_and_receive(_insert);
  ASSERT_EQ(static_cast<uint8_t>(_insert_response[0]), 1);

  const auto _query = request_query_builder(_key);
  for (int i = 0; i < 3; ++i)
  {
    auto _query_response = send_and_receive(_query, 2 + (sizeof(value_type) * 2));
    ASSERT_EQ(static_cast<uint8_t>(_query_response[0]), 1);
  }

  const auto _update1 = request_update_builder(attribute_types::quota, change_types::decrease, 10, _key);
  const auto _update2 = request_update_builder(attribute_types::quota, change_types::increase, 5, _key);
  auto _resp1 = send_and_receive(_update1);
  auto _resp2 = send_and_receive(_update2);
  ASSERT_EQ(static_cast<uint8_t>(_resp1[0]), 1);
  ASSERT_EQ(static_cast<uint8_t>(_resp2[0]), 1);

  std::this_thread::sleep_for(std::chrono::seconds(65));

  const auto _stat = request_stat_builder(_key);
  auto _stat_response = send_and_receive(_stat, 1 + (sizeof(uint64_t) * 4));
  ASSERT_EQ(_stat_response.size(), 1 + (sizeof(uint64_t) * 4));

  ASSERT_EQ(static_cast<uint8_t>(_stat_response[0]), 1);

#ifdef ENABLED_FEATURE_METRIC
  uint64_t _rpm = 0;
  uint64_t _wpm = 0;
  uint64_t _reads_total = 0;
  uint64_t _writes_total = 0;
  std::memcpy(&_rpm, _stat_response.data() + 1, sizeof(uint64_t));
  std::memcpy(&_wpm, _stat_response.data() + 1 + sizeof(uint64_t), sizeof(uint64_t));
  std::memcpy(&_reads_total, _stat_response.data() + 1 + sizeof(uint64_t) * 2, sizeof(uint64_t));
  std::memcpy(&_writes_total, _stat_response.data() + 1 + sizeof(uint64_t) * 3, sizeof(uint64_t));
  S
    // Verificar que haya al menos 3 lecturas y 2 escrituras
    ASSERT_GE(_rpm, 3);
  ASSERT_GE(_wpm, 2);
  ASSERT_EQ(_reads_total, _rpm);
  ASSERT_EQ(_writes_total, _wpm);
#endif
  boost::system::error_code _ec;
  _socket.close(_ec);
}
