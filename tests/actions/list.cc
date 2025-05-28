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

class ListTestFixture : public ServiceTestFixture
{
};

TEST_F(ListTestFixture, OnSuccessSingleFragment)
{
  const std::string _key1 = "abc";
  const std::string _key2 = "EHLO";
  const std::vector _value1 = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04}};
  const std::vector _value2 =
    {std::byte{0x05},
     std::byte{0x06},
     std::byte{0x07},
     std::byte{0x08},
     std::byte{0x09},
     std::byte{0x0A},
     std::byte{0x0B},
     std::byte{0x0C}};

  // SET key1 (counter, type 0x00)
  const auto _buffer1 = request_set_builder(_value1, ttl_types::seconds, 10, _key1);
  const auto _response1 = send_and_receive(_buffer1);
  ASSERT_EQ(static_cast<uint8_t>(_response1[0]), 1);

  // SET key2 (buffer, type 0x01)
  const auto _buffer2 = request_set_builder(_value2, ttl_types::seconds, 10, _key2);
  const auto _response2 = send_and_receive(_buffer2);
  ASSERT_EQ(static_cast<uint8_t>(_response2[0]), 1);

  // LIST request
  const auto _list_buffer = request_list_builder();
  const auto _response =
    send_and_receive(_list_buffer, 1 + 8 + 16 + ((11 + sizeof(value_type)) * 2) + _key1.size() + _key2.size());

  size_t _offset = 1;

  // Fragment count
  uint64_t _fragment_count;
  std::memcpy(&_fragment_count, _response.data() + _offset, sizeof(_fragment_count));
  _offset += sizeof(_fragment_count);
  ASSERT_EQ(_fragment_count, 1);

  // Fragment ID
  uint64_t _fragment_id;
  std::memcpy(&_fragment_id, _response.data() + _offset, sizeof(_fragment_id));
  _offset += sizeof(_fragment_id);
  ASSERT_EQ(_fragment_id, 1);

  // Key count
  uint64_t _key_count;
  std::memcpy(&_key_count, _response.data() + _offset, sizeof(_key_count));
  _offset += sizeof(_key_count);
  ASSERT_EQ(_key_count, 2);

  // Key 1 metadata
  ASSERT_EQ(static_cast<uint8_t>(_response[_offset]), _key1.size());
  _offset += 1;
  ASSERT_EQ(static_cast<uint8_t>(_response[_offset]), 0x01);
  _offset += 1; // type
  ASSERT_EQ(static_cast<uint8_t>(_response[_offset]), static_cast<uint8_t>(ttl_types::seconds));
  _offset += 1;

  uint64_t _expires1;
  std::memcpy(&_expires1, _response.data() + _offset, sizeof(_expires1));
  _offset += sizeof(_expires1);
  ASSERT_GT(_expires1, 0);

  value_type _bytes_used_1;
  std::memcpy(&_bytes_used_1, _response.data() + _offset, sizeof(_bytes_used_1));
  _offset += sizeof(_bytes_used_1);
  ASSERT_EQ(_bytes_used_1, _value1.size());

  // Key 2 metadata
  ASSERT_EQ(static_cast<uint8_t>(_response[_offset]), _key2.size());
  _offset += 1;
  ASSERT_EQ(static_cast<uint8_t>(_response[_offset]), 0x01);
  _offset += 1; // type
  ASSERT_EQ(static_cast<uint8_t>(_response[_offset]), static_cast<uint8_t>(ttl_types::seconds));
  _offset += 1;

  uint64_t _expires2;
  std::memcpy(&_expires2, _response.data() + _offset, sizeof(_expires2));
  _offset += sizeof(_expires2);
  ASSERT_GT(_expires2, 0);

  value_type _bytes_used_2;
  std::memcpy(&_bytes_used_2, _response.data() + _offset, sizeof(_bytes_used_2));
  _offset += sizeof(_bytes_used_2);
  ASSERT_EQ(_bytes_used_2, _value2.size());

  // Key 1 raw
  ASSERT_EQ(std::memcmp(_response.data() + _offset, _key1.data(), _key1.size()), 0);
  _offset += _key1.size();

  // Key 2 raw
  ASSERT_EQ(std::memcmp(_response.data() + _offset, _key2.data(), _key2.size()), 0);
  _offset += _key2.size();

  ASSERT_EQ(_offset, _response.size());
}

TEST_F(ListTestFixture, OnSuccessMultipleFragments)
{
  std::vector<std::string> _keys;
  std::vector<std::vector<std::byte>> _values;

  for (int _i = 0; _i < 100; ++_i)
  {
    std::string _key = "key_" + std::to_string(_i); // NOSONAR
    _key += std::string(10 - _key.size(), 'X');
    _keys.push_back(_key);

    std::vector _value = {std::byte{0xAB}, std::byte{0xCD}};
    _values.push_back(_value);

    const auto _buffer = request_set_builder(_value, ttl_types::seconds, 60, _key);
    const auto _response = send_and_receive(_buffer);
    ASSERT_EQ(static_cast<uint8_t>(_response[0]), 1);
  }

  const auto _list_buffer = request_list_builder();

  boost::asio::io_context _io_context;
  tcp::resolver _resolver(_io_context);
  const auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(app_->state_->exposed_port_));
  tcp::socket _socket(_io_context);
  boost::asio::connect(_socket, _endpoints);
  boost::asio::write(_socket, boost::asio::buffer(_list_buffer.data(), _list_buffer.size()));

  uint8_t _status = 0;
  boost::asio::read(_socket, boost::asio::buffer(&_status, 1));
  ASSERT_EQ(_status, 0x01);

  uint64_t _fragment_count = 0;
  boost::asio::read(_socket, boost::asio::buffer(&_fragment_count, 8));
  ASSERT_GE(_fragment_count, 2);

  std::vector<std::byte> _full_response;
  _full_response.insert(
    _full_response.end(),
    reinterpret_cast<std::byte *>(&_fragment_count),
    reinterpret_cast<std::byte *>(&_fragment_count) + sizeof(_fragment_count));

  uint64_t _total_keys = 0;
  std::vector<size_t> _all_key_sizes;
  std::vector<std::string> _read_keys;

  for (uint64_t _i = 0; _i < _fragment_count; ++_i)
  {
    uint64_t _fragment_id = 0;
    uint64_t _key_count = 0;

    boost::asio::read(_socket, boost::asio::buffer(&_fragment_id, sizeof(_fragment_id)));
    boost::asio::read(_socket, boost::asio::buffer(&_key_count, sizeof(_key_count)));

    _full_response.insert(
      _full_response.end(),
      reinterpret_cast<std::byte *>(&_fragment_id),
      reinterpret_cast<std::byte *>(&_fragment_id) + sizeof(_fragment_id));
    _full_response.insert(
      _full_response.end(),
      reinterpret_cast<std::byte *>(&_key_count),
      reinterpret_cast<std::byte *>(&_key_count) + sizeof(_key_count));

    _total_keys += _key_count;
    std::vector<size_t> _fragment_key_sizes;

    for (uint64_t _k = 0; _k < _key_count; ++_k)
    {
      std::array<std::byte, 11 + sizeof(value_type)> _meta{};
      boost::asio::read(_socket, boost::asio::buffer(_meta));
      _full_response.insert(_full_response.end(), _meta.begin(), _meta.end());

      const auto _key_size = static_cast<size_t>(_meta[0]);
      _fragment_key_sizes.push_back(_key_size);
      _all_key_sizes.push_back(_key_size);
    }

    for (size_t _k = 0; _k < _fragment_key_sizes.size(); ++_k) // NOSONAR
    {
      std::vector<std::byte> _key(_fragment_key_sizes[_k]);
      boost::asio::read(_socket, boost::asio::buffer(_key));
      _full_response.insert(_full_response.end(), _key.begin(), _key.end());
      std::string _string_key(reinterpret_cast<const char *>(_key.data()), _key.size()); // NOSONAR
      _read_keys.push_back(_string_key);
    }
  }

  std::sort(_keys.begin(), _keys.end());           // NOSONAR
  std::sort(_read_keys.begin(), _read_keys.end()); // NOSONAR

  ASSERT_EQ(_read_keys.size(), _keys.size());

  for (size_t _i = 0; _i < _keys.size(); ++_i)
  {
    ASSERT_EQ(_read_keys[_i], _keys[_i]);
  }
}
