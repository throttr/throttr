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

class GetTestFixture : public ServiceTestFixture
{
};

TEST_F(GetTestFixture, OnSuccess)
{
  const std::string _key = "consumer/get_test";
  const std::vector<std::byte> _value = {std::byte{0xBA}, std::byte{0xAD}, std::byte{0xF0}, std::byte{0x0D}};

  // 1. SET
  const auto _set_buffer = request_set_builder(_value, ttl_types::seconds, 3, _key);
  const auto _set_response = send_and_receive(_set_buffer);
  ASSERT_EQ(_set_response.size(), 1);
  ASSERT_EQ(static_cast<uint8_t>(_set_response[0]), 1);

  // 2. GET
  const auto _get_buffer = request_get_builder(_key);
  const auto _get_response = send_and_receive(_get_buffer, 1 + 1 + sizeof(value_type) * 2 + _value.size());

  ASSERT_EQ(_get_response.size(), 1 + 1 + sizeof(value_type) * 2 + _value.size());

  // Verifica status
  ASSERT_EQ(static_cast<uint8_t>(_get_response[0]), 1);

  // ttl_type
  ASSERT_EQ(static_cast<uint8_t>(_get_response[1]), static_cast<uint8_t>(ttl_types::seconds));

  // ttl
  value_type _ttl;
  std::memcpy(&_ttl, _get_response.data() + 2, sizeof(_ttl));
  ASSERT_GT(_ttl, 0);

  // value_size
  value_type _value_size;
  std::memcpy(&_value_size, _get_response.data() + 2 + sizeof(_ttl), sizeof(_value_size));
  ASSERT_EQ(_value_size, _value.size());

  // value
  const auto *_value_ptr = _get_response.data() + 2 + sizeof(_ttl) + sizeof(_value_size);
  for (std::size_t i = 0; i < _value.size(); ++i)
  {
    ASSERT_EQ(_value_ptr[i], _value[i]);
  }
}