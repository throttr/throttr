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

class BatchProcessingTestFixture : public ServiceTestFixture
{
};

TEST_F(BatchProcessingTestFixture, OnSuccess)
{
  const auto buffer1 = request_insert_builder(1, ttl_types::seconds, 64, "consumer3/resource3");
  const auto buffer2 = request_insert_builder(1, ttl_types::seconds, 64, "consumer4/resource4");

  std::vector<std::byte> batch;
  batch.insert(batch.end(), buffer1.begin(), buffer1.end());
  batch.insert(batch.end(), buffer2.begin(), buffer2.end());

  auto responses = send_and_receive(batch, 2);
  ASSERT_EQ(responses.size(), 2);

  ASSERT_EQ(static_cast<uint8_t>(responses[0]), 1);
  ASSERT_EQ(static_cast<uint8_t>(responses[1]), 1);
}
