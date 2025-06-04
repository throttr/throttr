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

class SetTestFixture : public ServiceTestFixture
{
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

TEST_F(SetTestFixture, OnSuccessRetry)
{
  const std::string _key = "consumer/set_again_value";
  const std::vector _value = {std::byte{0xBE}, std::byte{0xEF}, std::byte{0xCA}, std::byte{0xFE}};
  const std::vector _new_value = {std::byte{0xFE}, std::byte{0xCA}, std::byte{0xEF}, std::byte{0xBE}};

  const auto _buffer = request_set_builder(_value, ttl_types::seconds, 10, _key);

  auto _response = send_and_receive(_buffer);

  ASSERT_EQ(_response.size(), 1);
  ASSERT_EQ(static_cast<uint8_t>(_response[0]), 1);

  const auto _retry_buffer = request_set_builder(_new_value, ttl_types::minutes, 60, _key);

  auto _retry_response = send_and_receive(_retry_buffer);

  ASSERT_EQ(_response.size(), 1);
  ASSERT_EQ(static_cast<uint8_t>(_response[0]), 1);

  const auto _get_buffer = request_get_builder(_key);
  const auto _get_response = send_and_receive(_get_buffer, 1 + 1 + sizeof(value_type) * 2 + _value.size());

  ASSERT_EQ(_get_response.size(), 1 + 1 + sizeof(value_type) * 2 + _value.size());

  // Verifica status
  ASSERT_EQ(static_cast<uint8_t>(_get_response[0]), 1);

  // ttl_type
  ASSERT_EQ(static_cast<uint8_t>(_get_response[1]), static_cast<uint8_t>(ttl_types::minutes));

  // ttl
  value_type _ttl;
  std::memcpy(&_ttl, _get_response.data() + 2, sizeof(_ttl));
  _ttl = boost::endian::little_to_native(_ttl);
  ASSERT_LE(_ttl, 60);
  ASSERT_GE(_ttl, 50);

  // value_size
  value_type _value_size;
  std::memcpy(&_value_size, _get_response.data() + 2 + sizeof(_ttl), sizeof(_value_size));
  _value_size = boost::endian::little_to_native(_value_size);
  ASSERT_EQ(_value_size, _value.size());

  // value
  const auto *_value_ptr = _get_response.data() + 2 + sizeof(_ttl) + sizeof(_value_size);
  for (std::size_t i = 0; i < _new_value.size(); ++i)
  {
    ASSERT_EQ(_value_ptr[i], _new_value[i]);
  }
}