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

#include <boost/core/ignore_unused.hpp>

class QueryTestFixture : public ServiceTestFixture
{
};

TEST_F(QueryTestFixture, OnSuccess)
{
  const auto _insert_buffer = request_insert_builder(10, ttl_types::seconds, 16, "consumer_query2/resource_query2");

  auto _ignored = send_and_receive(_insert_buffer);
  boost::ignore_unused(_ignored);

  const auto _query_buffer = request_query_builder("consumer_query2/resource_query2");
  auto _response = send_and_receive(_query_buffer, 2 + (sizeof(value_type) * 2));

  ASSERT_EQ(static_cast<uint8_t>(_response[0]), 1);

  value_type _quota_remaining = 0;
  std::memcpy(&_quota_remaining, _response.data() + 1, sizeof(_quota_remaining));
  _quota_remaining = boost::endian::little_to_native(_quota_remaining);
  ASSERT_EQ(_quota_remaining, 10);
}

TEST_F(QueryTestFixture, OnFailedDueNonExistingKey)
{
  const auto _buffer = request_query_builder("consumer_query/resource_query");
  auto _response = send_and_receive(_buffer);

  ASSERT_EQ(_response.size(), 1);
  ASSERT_EQ(static_cast<uint8_t>(_response[0]), 0);
}

TEST_F(QueryTestFixture, OnFailedDueExpiredKey)
{
  const auto _insert_buffer = request_insert_builder(0, ttl_types::seconds, 1, "consumer_query3/resource_query3");

  auto _ignored = send_and_receive(_insert_buffer);
  boost::ignore_unused(_ignored);

  std::this_thread::sleep_for(std::chrono::milliseconds(1500));
  const auto _query_buffer = request_query_builder("consumer_query3/resource_query3");

  auto _response = send_and_receive(_query_buffer);
  ASSERT_EQ(static_cast<uint8_t>(_response[0]), 0);
}

TEST_F(QueryTestFixture, OnSuccessUntilExpired)
{
  const auto _insert_buffer = request_insert_builder(32, ttl_types::seconds, 3, "consumer3/expire");

  auto _ignored = send_and_receive(_insert_buffer);
  boost::ignore_unused(_ignored);

  const auto _query_buffer = request_query_builder("consumer3/expire");
  auto _success_query_response = send_and_receive(_query_buffer, 2 + (sizeof(value_type) * 2));

  ASSERT_EQ(static_cast<uint8_t>(_success_query_response[0]), 1);

  value_type _success_response_quota;
  std::memcpy(&_success_response_quota, _success_query_response.data() + 1,
              sizeof(value_type)); // 2 bytes
  _success_response_quota = boost::endian::little_to_native(_success_response_quota);
  ASSERT_EQ(_success_response_quota, 32);

  const auto _success_response_ttl_type = static_cast<uint8_t>(_success_query_response[sizeof(value_type) + 1]);
  ASSERT_EQ(_success_response_ttl_type, static_cast<uint8_t>(ttl_types::seconds));

  value_type _success_response_ttl;

  std::memcpy(&_success_response_ttl, _success_query_response.data() + sizeof(value_type) + 2, sizeof(value_type));
  _success_response_ttl = boost::endian::little_to_native(_success_response_ttl);
  ASSERT_GT(_success_response_ttl, 0);

  std::this_thread::sleep_for(std::chrono::milliseconds(3100));

  auto _expired_query_response = send_and_receive(_query_buffer);
  ASSERT_EQ(static_cast<uint8_t>(_expired_query_response[0]), 0);
}
