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

class SetTestFixture : public ServiceTestFixture{
};

TEST_F(SetTestFixture, OnSuccess)
{
    const std::string _key = "consumer/set_value";
    const std::vector _value = {std::byte{0xBE}, std::byte{0xEF}, std::byte{0xCA}, std::byte{0xFE}};

    const auto _buffer = request_set_builder(_value, ttl_types::seconds, 10, _key);

    auto _response = send_and_receive(_buffer);

    ASSERT_EQ(_response.size(), 1);
    ASSERT_EQ(static_cast<uint8_t>(_response[0]), 1);
}