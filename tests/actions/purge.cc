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

class PurgeTestFixture : public ServiceTestFixture
{
};

TEST_F(PurgeTestFixture, OnSuccess)
{
  const auto _insert = request_insert_builder(1, ttl_types::seconds, 32, "consumer_purge/resource_purge");

  auto _ignored = send_and_receive(_insert);
  boost::ignore_unused(_ignored);

  const auto _purge = request_purge_builder("consumer_purge/resource_purge");
  auto _purge_response = send_and_receive(_purge);
  ASSERT_EQ(static_cast<uint8_t>(_purge_response[0]), 1);

  const auto _query = request_query_builder("consumer_purge/resource_purge");
  const auto _query_response = send_and_receive(_query);
  ASSERT_EQ(_query_response.size(), 1);
  ASSERT_EQ(static_cast<uint8_t>(_query_response[0]), 0);
}

TEST_F(PurgeTestFixture, OnFailed)
{
  const auto _purge = request_purge_builder("nonexistent_consumer/nonexistent_resource");
  auto _purge_response = send_and_receive(_purge);

  ASSERT_EQ(_purge_response.size(), 1);

  ASSERT_EQ(static_cast<uint8_t>(_purge_response[0]), 0);
}
