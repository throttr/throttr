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

class UpdateTestFixture : public ServiceTestFixture{
};

TEST_F(UpdateTestFixture, IncreaseQuota)
{
  const auto _insert_buffer = request_insert_builder(0, ttl_types::seconds, 77, "consumer/increase_quota");

  auto _ignored = send_and_receive(_insert_buffer);
  boost::ignore_unused(_ignored);

  const auto _update_buffer =
    request_update_builder(attribute_types::quota, change_types::increase, 10, "consumer/increase_quota");
  auto _update_response = send_and_receive(_update_buffer);

  ASSERT_EQ(_update_response.size(), 1);
  ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);

  const auto _query_buffer = request_query_builder("consumer/increase_quota");
  auto _query_response = send_and_receive(_query_buffer, 2 + (sizeof(value_type) * 2));

  ASSERT_EQ(_query_response.size(), 2 + (sizeof(value_type) * 2));
  ASSERT_EQ(static_cast<uint8_t>(_query_response[0]), 1);

  value_type _quota = 0;
  std::memcpy(&_quota, _query_response.data() + 1, sizeof(_quota));
  ASSERT_EQ(_quota, 10);
}

TEST_F(UpdateTestFixture, DecreaseQuota)
{
  const auto _insert = request_insert_builder(10, ttl_types::seconds, 32, "consumer/decrease_quota");

  auto _ignored = send_and_receive(_insert);
  boost::ignore_unused(_ignored);

  const auto _update = request_update_builder(attribute_types::quota, change_types::decrease, 4, "consumer/decrease_quota");
  auto _update_response = send_and_receive(_update);
  ASSERT_EQ(_update_response.size(), 1);

  ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);

  const auto _query = request_query_builder("consumer/decrease_quota");
  const auto _query_response = send_and_receive(_query, 2 + (sizeof(value_type) * 2));

  value_type _quota = 0;
  std::memcpy(&_quota, _query_response.data() + 1, sizeof(_quota));
  ASSERT_EQ(_quota, 6);
}

TEST_F(UpdateTestFixture, PatchQuota)
{
  const auto _insert = request_insert_builder(10, ttl_types::seconds, 64, "consumer/patch_quota");

  auto _ignored = send_and_receive(_insert);
  boost::ignore_unused(_ignored);

  const auto _update = request_update_builder(attribute_types::quota, change_types::patch, 4, "consumer/patch_quota");
  auto _update_response = send_and_receive(_update);
  ASSERT_EQ(_update_response.size(), 1);

  ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);

  const auto _query = request_query_builder("consumer/patch_quota");
  const auto _query_response = send_and_receive(_query, 2 + (sizeof(value_type) * 2));

  value_type _quota = 0;
  std::memcpy(&_quota, _query_response.data() + 1, sizeof(_quota));
  ASSERT_EQ(_quota, 4);
}

TEST_F(UpdateTestFixture, PatchTTL)
{
  const auto _insert = request_insert_builder(10, ttl_types::seconds, 5, "consumer/patch_ttl");

  auto _ignored = send_and_receive(_insert);
  boost::ignore_unused(_ignored);

  const auto _update = request_update_builder(attribute_types::ttl, change_types::patch, 64, "consumer/patch_ttl");
  auto _update_response = send_and_receive(_update);
  ASSERT_EQ(_update_response.size(), 1);
  ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);

  const auto _query = request_query_builder("consumer/patch_ttl");
  auto _query_response = send_and_receive(_query, 2 + (sizeof(value_type) * 2));

  ASSERT_EQ(_query_response.size(), 2 + (sizeof(value_type) * 2));
  ASSERT_EQ(static_cast<uint8_t>(_query_response[0]), 1);

  value_type _quota = 0;
  std::memcpy(&_quota, _query_response.data() + 1, sizeof(_quota));
  ASSERT_EQ(_quota, 10);

  const auto _ttl_type = static_cast<uint8_t>(_query_response[1 + sizeof(_quota)]);
  ASSERT_EQ(_ttl_type, static_cast<uint8_t>(ttl_types::seconds));

  value_type _ttl = 0;
  std::memcpy(&_ttl, _query_response.data() + 2 + sizeof(_quota), sizeof(_ttl));
  ASSERT_GE(_ttl, 0);
  ASSERT_LE(_ttl, 64);
}

TEST_F(UpdateTestFixture, OnFailed)
{
  const auto _update = request_update_builder(attribute_types::quota, change_types::patch, 100, "non_existing_resource");

  auto _update_response = send_and_receive(_update);
  ASSERT_EQ(_update_response.size(), 1);

  ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 0);
}

TEST_F(UpdateTestFixture, OnFailedDueQuotaConsumed)
{
  const auto _insert = request_insert_builder(0, ttl_types::seconds, 32, "consumer_beyond/resource_beyond");

  auto _ignored = send_and_receive(_insert);
  boost::ignore_unused(_ignored);

  const auto _update =
    request_update_builder(attribute_types::quota, change_types::decrease, 10, "consumer_beyond/resource_beyond");
  auto _update_response = send_and_receive(_update);
  ASSERT_EQ(_update_response.size(), 1);
  ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 0);
}

TEST_F(UpdateTestFixture, IncreaseTTL)
{
  const auto _insert = request_insert_builder(0, ttl_types::seconds, 32, "consumer_inc/resource_inc");
  auto _ignored = send_and_receive(_insert);
  boost::ignore_unused(_ignored);

  const auto _update = request_update_builder(attribute_types::ttl, change_types::increase, 18, "consumer_inc/resource_inc");
  auto _update_response = send_and_receive(_update);

  ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);
}

TEST_F(UpdateTestFixture, DecreaseTTL)
{
  const auto _insert = request_insert_builder(0, ttl_types::seconds, 32, "consumer_dec/resource_dec");
  auto _ignored = send_and_receive(_insert);
  boost::ignore_unused(_ignored);

  const auto _update = request_update_builder(attribute_types::ttl, change_types::decrease, 12, "consumer_dec/resource_dec");
  auto _update_response = send_and_receive(_update);

  ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);
}