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

class InsertTestFixture : public ServiceTestFixture {
};

TEST_F(InsertTestFixture, OnSuccess)
{
    const auto _buffer = request_insert_builder(1, ttl_types::seconds, 32, "consumer1/resource1");
    auto _response = send_and_receive(_buffer);

    ASSERT_EQ(_response.size(), 1);
    ASSERT_EQ(static_cast<uint8_t>(_response[0]), 1);
}

TEST_F(InsertTestFixture, OnSuccessOnDifferentKeys)
{
    const auto _buffer_a = request_insert_builder(3, ttl_types::seconds, 7, "consumerA/resourceA");
    const auto _buffer_b = request_insert_builder(5, ttl_types::seconds, 7, "consumerB/resourceB");

    const auto _response_a1 = send_and_receive(_buffer_a);
    ASSERT_EQ(_response_a1.size(), 1);
    ASSERT_EQ(static_cast<uint8_t>(_response_a1[0]), 1);

    const auto _response_b1 = send_and_receive(_buffer_b);
    ASSERT_EQ(_response_b1.size(), 1);
    ASSERT_EQ(static_cast<uint8_t>(_response_b1[0]), 1);
}

TEST_F(InsertTestFixture, OnFailedDueAlreadyCreatedKey)
{
    const auto _buffer = request_insert_builder(1, ttl_types::seconds, 32, "consumer2/resource2");

    for (int _i = 0; _i < 5; ++_i)
    {
        auto _response = send_and_receive(_buffer);
        ASSERT_EQ(_response.size(), 1);

        if (_i == 0)
        {
            ASSERT_EQ(static_cast<uint8_t>(_response[0]), 1);
        }
        else
        {
            ASSERT_EQ(static_cast<uint8_t>(_response[0]), 0);
        }
    }
}
