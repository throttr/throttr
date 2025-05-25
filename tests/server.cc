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

#include <gtest/gtest.h>
#include <numeric>

#include <boost/asio.hpp>
#include <thread>
#include <throttr/app.hpp>
#include <throttr/state.hpp>

using boost::asio::ip::tcp;
using namespace throttr;

class ServerTestFixture : public testing::Test
{
  std::unique_ptr<std::jthread> server_thread_;
  std::shared_ptr<app> app_;
  int threads_ = 1;

protected:
  void SetUp() override
  {
    app_ = std::make_shared<app>(1337, threads_);

    server_thread_ = std::make_unique<std::jthread>([this]() { app_->serve(); });

    while (!app_->state_->acceptor_ready_)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  void TearDown() override
  {
    app_->stop();

    if (server_thread_ && server_thread_->joinable())
    {
      server_thread_->join();
    }
  }

  [[nodiscard]] static std::vector<std::byte> send_and_receive(const std::vector<std::byte> &message, const int length = 1)
  {
    boost::asio::io_context _io_context;
    tcp::resolver _resolver(_io_context);

    const auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(1337));

    tcp::socket _socket(_io_context);
    boost::asio::connect(_socket, _endpoints);

    boost::asio::write(_socket, boost::asio::buffer(message.data(), message.size()));

    std::vector<std::byte> _response(length);
    boost::asio::read(_socket, boost::asio::buffer(_response.data(), _response.size()));

    return _response;
  }
};

TEST_F(ServerTestFixture, HandlesSingleValidRequest)
{
  const auto _buffer = request_insert_builder(1, ttl_types::seconds, 32, "consumer1/resource1");
  auto _response = send_and_receive(_buffer);

  ASSERT_EQ(_response.size(), 1);
  ASSERT_EQ(static_cast<uint8_t>(_response[0]), 1);
}

TEST_F(ServerTestFixture, HandlesMultipleSingleRequests)
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

TEST_F(ServerTestFixture, HandlesTwoConcatenatedRequests)
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

TEST_F(ServerTestFixture, HandleSetSimpleValue)
{
  const std::string _key = "consumer/set_value";
  const std::vector _value = {std::byte{0xBE}, std::byte{0xEF}, std::byte{0xCA}, std::byte{0xFE}};

  const auto _buffer = request_set_builder(_value, ttl_types::seconds, 10, _key);

  auto _response = send_and_receive(_buffer);

  ASSERT_EQ(_response.size(), 1);
  ASSERT_EQ(static_cast<uint8_t>(_response[0]), 1);
}

TEST_F(ServerTestFixture, HandleGetReturnsCorrectValue)
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

TEST_F(ServerTestFixture, TTLExpiration)
{
  const auto _insert_buffer = request_insert_builder(32, ttl_types::milliseconds, 28, "consumer3/expire");

  auto _ignored = send_and_receive(_insert_buffer);
  boost::ignore_unused(_ignored);

  const auto _query_buffer = request_query_builder("consumer3/expire");
  auto _success_query_response = send_and_receive(_query_buffer, 2 + (sizeof(value_type) * 2));

  // [x] [o][o] [o] [o][o]
  ASSERT_EQ(static_cast<uint8_t>(_success_query_response[0]), 1);

  value_type _success_response_quota;

  // [o] [x][x] [o] [o][o]
  std::memcpy(&_success_response_quota, _success_query_response.data() + 1,
              sizeof(value_type)); // 2 bytes
  ASSERT_EQ(_success_response_quota, 32);

  // [o] [o][o] [x] [o][o]
  const auto _success_response_ttl_type = static_cast<uint8_t>(_success_query_response[sizeof(value_type) + 1]);
  ASSERT_EQ(_success_response_ttl_type, static_cast<uint8_t>(ttl_types::milliseconds));

  value_type _success_response_ttl;

  // [o] [o][o] [o] [x][x]
  std::memcpy(&_success_response_ttl, _success_query_response.data() + sizeof(value_type) + 2, sizeof(value_type));
  ASSERT_GT(_success_response_ttl, 0);
  ;

  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  auto _expired_query_response = send_and_receive(_query_buffer);

  ASSERT_EQ(static_cast<uint8_t>(_expired_query_response[0]), 0);
}

TEST_F(ServerTestFixture, SeparateStocksForDifferentIDs)
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

TEST_F(ServerTestFixture, QueryBeforeInsertReturnsZeroQuota)
{
  const auto _buffer = request_query_builder("consumer_query/resource_query");
  auto _response = send_and_receive(_buffer);

  ASSERT_EQ(_response.size(), 1);
  ASSERT_EQ(static_cast<uint8_t>(_response[0]), 0);
}

TEST_F(ServerTestFixture, QueryAfterInsertReturnsCorrectQuota)
{
  const auto _insert_buffer = request_insert_builder(10, ttl_types::seconds, 16, "consumer_query2/resource_query2");

  auto _ignored = send_and_receive(_insert_buffer);
  boost::ignore_unused(_ignored);

  const auto _query_buffer = request_query_builder("consumer_query2/resource_query2");
  auto _response = send_and_receive(_query_buffer, 2 + (sizeof(value_type) * 2));

  ASSERT_EQ(static_cast<uint8_t>(_response[0]), 1);

  value_type _quota_remaining = 0;
  std::memcpy(&_quota_remaining, _response.data() + 1, sizeof(_quota_remaining));
  ASSERT_EQ(_quota_remaining, 10);
}

TEST_F(ServerTestFixture, QueryExpiredReturnsZeroQuota)
{
  const auto _insert_buffer = request_insert_builder(0, ttl_types::seconds, 1, "consumer_query3/resource_query3");

  auto _ignored = send_and_receive(_insert_buffer);
  boost::ignore_unused(_ignored);

  std::this_thread::sleep_for(std::chrono::seconds(1));
  const auto _query_buffer = request_query_builder("consumer_query3/resource_query3");

  auto _response = send_and_receive(_query_buffer);
  ASSERT_EQ(static_cast<uint8_t>(_response[0]), 0);
}

TEST_F(ServerTestFixture, UpdateIncreaseQuota)
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

TEST_F(ServerTestFixture, UpdateDecreaseQuota)
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

TEST_F(ServerTestFixture, UpdatePatchQuota)
{
  const auto _insert = request_insert_builder(10, ttl_types::milliseconds, 64, "consumer/patch_quota");

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

TEST_F(ServerTestFixture, UpdatePatchTTL)
{
  const auto _insert = request_insert_builder(10, ttl_types::milliseconds, 32, "consumer/patch_ttl");

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
  ASSERT_EQ(_ttl_type, static_cast<uint8_t>(ttl_types::milliseconds));

  value_type _ttl = 0;
  std::memcpy(&_ttl, _query_response.data() + 2 + sizeof(_quota), sizeof(_ttl));
  ASSERT_GE(_ttl, 0);
  ASSERT_LE(_ttl, 64);
}

TEST_F(ServerTestFixture, UpdateNonExistentKeyReturnsError)
{
  const auto _update = request_update_builder(attribute_types::quota, change_types::patch, 100, "non_existing_resource");

  auto _update_response = send_and_receive(_update);
  ASSERT_EQ(_update_response.size(), 1);

  ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 0);
}

TEST_F(ServerTestFixture, UpdateDecreaseQuotaBeyondZero)
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

TEST_F(ServerTestFixture, UpdatePatchTTLSeconds)
{
  const auto _insert = request_insert_builder(0, ttl_types::seconds, 5, "consumer_sec/resource_sec");

  auto _ignored = send_and_receive(_insert);
  boost::ignore_unused(_ignored);

  const auto _update = request_update_builder(attribute_types::ttl, change_types::patch, 10, "consumer_sec/resource_sec");
  std::vector<std::byte> _update_response = send_and_receive(_update);
  ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);
}

TEST_F(ServerTestFixture, UpdateIncreaseTTL)
{
  const auto _insert = request_insert_builder(0, ttl_types::milliseconds, 32, "consumer_inc/resource_inc");
  auto _ignored = send_and_receive(_insert);
  boost::ignore_unused(_ignored);

  const auto _update = request_update_builder(attribute_types::ttl, change_types::increase, 18, "consumer_inc/resource_inc");
  auto _update_response = send_and_receive(_update);

  ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);
}

TEST_F(ServerTestFixture, UpdateDecreaseTTL)
{
  const auto _insert = request_insert_builder(0, ttl_types::milliseconds, 32, "consumer_dec/resource_dec");
  auto _ignored = send_and_receive(_insert);
  boost::ignore_unused(_ignored);

  const auto _update = request_update_builder(attribute_types::ttl, change_types::decrease, 12, "consumer_dec/resource_dec");
  auto _update_response = send_and_receive(_update);

  ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);
}

TEST_F(ServerTestFixture, PurgeExistingEntry)
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

TEST_F(ServerTestFixture, HandlesListReturnsCorrectStructure)
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
  const auto _response = send_and_receive(_list_buffer, 8 + 16 + ((11 + sizeof(value_type)) * 2) + _key1.size() + _key2.size());

  size_t _offset = 0;

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

  std::cout << span_to_hex(_response) << std::endl;

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

TEST_F(ServerTestFixture, HandlesListWithMultipleFragments)
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
  const auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(1337));
  tcp::socket _socket(_io_context);
  boost::asio::connect(_socket, _endpoints);
  boost::asio::write(_socket, boost::asio::buffer(_list_buffer.data(), _list_buffer.size()));
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

TEST_F(ServerTestFixture, HandlesStatsReturnsCorrectStructure)
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

  // SET key1
  const auto _buffer1 = request_set_builder(_value1, ttl_types::seconds, 10, _key1);
  const auto _response1 = send_and_receive(_buffer1);
  ASSERT_EQ(static_cast<uint8_t>(_response1[0]), 1);

  // SET key2
  const auto _buffer2 = request_set_builder(_value2, ttl_types::seconds, 10, _key2);
  const auto _response2 = send_and_receive(_buffer2);
  ASSERT_EQ(static_cast<uint8_t>(_response2[0]), 1);

  // STATS request
  const auto _stats_buffer = request_stats_builder();
  const auto _response = send_and_receive(
    _stats_buffer,
    8 +                 // fragment count
      8 +               // fragment id
      8 +               // key count
      2 * (1 + 8 * 4) + // metadata for 2 keys
      _key1.size() + _key2.size());

  size_t _offset = 0;

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

  // Key 1 metrics
  ASSERT_EQ(static_cast<uint8_t>(_response[_offset]), _key1.size());
  _offset += 1;

  for (int _i = 0; _i < 4; ++_i)
  {
    uint64_t _metric;
    std::memcpy(&_metric, _response.data() + _offset, sizeof(_metric));
    ASSERT_GE(_metric, 0);
    _offset += sizeof(_metric);
  }

  // Key 2 metrics
  ASSERT_EQ(static_cast<uint8_t>(_response[_offset]), _key2.size());
  _offset += 1;

  for (int _i = 0; _i < 4; ++_i)
  {
    uint64_t _metric;
    std::memcpy(&_metric, _response.data() + _offset, sizeof(_metric));
    ASSERT_GE(_metric, 0);
    _offset += sizeof(_metric);
  }

  // Key 1 raw
  ASSERT_EQ(std::memcmp(_response.data() + _offset, _key1.data(), _key1.size()), 0);
  _offset += _key1.size();

  // Key 2 raw
  ASSERT_EQ(std::memcmp(_response.data() + _offset, _key2.data(), _key2.size()), 0);
  _offset += _key2.size();

  ASSERT_EQ(_offset, _response.size());
}

TEST_F(ServerTestFixture, HandlesStatsWithMultipleFragments)
{
  std::vector<std::string> _keys;
  std::vector<std::vector<std::byte>> _values;

  for (int _i = 0; _i < 100; ++_i)
  {
    std::string _key = "key_" + std::to_string(_i);
    _key += std::string(10 - _key.size(), 'X');
    _keys.push_back(_key);

    std::vector<std::byte> _value = {std::byte{0xAB}, std::byte{0xCD}};
    _values.push_back(_value);

    const auto _buffer = request_set_builder(_value, ttl_types::seconds, 60, _key);
    const auto _response = send_and_receive(_buffer);
    ASSERT_EQ(static_cast<uint8_t>(_response[0]), 1);
  }

  const auto _stats_buffer = request_stats_builder();

  boost::asio::io_context _io_context;
  tcp::resolver _resolver(_io_context);
  const auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(1337));
  tcp::socket _socket(_io_context);
  boost::asio::connect(_socket, _endpoints);
  boost::asio::write(_socket, boost::asio::buffer(_stats_buffer.data(), _stats_buffer.size()));

  uint64_t _fragment_count = 0;
  boost::asio::read(_socket, boost::asio::buffer(&_fragment_count, 8));
  ASSERT_GE(_fragment_count, 2);

  std::vector<std::byte> _full_response;
  _full_response.insert(
    _full_response.end(),
    reinterpret_cast<std::byte *>(&_fragment_count),
    reinterpret_cast<std::byte *>(&_fragment_count) + sizeof(_fragment_count));

  std::vector<std::string> _read_keys;
  std::vector<size_t> _all_key_sizes;

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

    std::vector<size_t> _fragment_key_sizes;

    for (uint64_t _k = 0; _k < _key_count; ++_k)
    {
      std::array<std::byte, 33> _meta{}; // 1 + 4*8
      boost::asio::read(_socket, boost::asio::buffer(_meta));
      _full_response.insert(_full_response.end(), _meta.begin(), _meta.end());

      const auto _key_size = static_cast<size_t>(_meta[0]);
      _fragment_key_sizes.push_back(_key_size);
      _all_key_sizes.push_back(_key_size);

      // Opcional: validar métricas no negativas
      for (int j = 0; j < 4; ++j)
      {
        uint64_t _metric;
        std::memcpy(&_metric, _meta.data() + 1 + j * 8, 8);
        ASSERT_GE(_metric, 0);
      }
    }

    for (size_t _k = 0; _k < _fragment_key_sizes.size(); ++_k)
    {
      std::vector<std::byte> _key(_fragment_key_sizes[_k]);
      boost::asio::read(_socket, boost::asio::buffer(_key));
      _full_response.insert(_full_response.end(), _key.begin(), _key.end());
      std::string _string_key(reinterpret_cast<const char *>(_key.data()), _key.size());
      _read_keys.push_back(_string_key);
    }
  }

  std::sort(_keys.begin(), _keys.end());
  std::sort(_read_keys.begin(), _read_keys.end());

  ASSERT_EQ(_read_keys.size(), _keys.size());

  for (size_t _i = 0; _i < _keys.size(); ++_i)
  {
    ASSERT_EQ(_read_keys[_i], _keys[_i]);
  }
}

TEST_F(ServerTestFixture, PurgeNonExistentEntryReturnsError)
{
  const auto _purge = request_purge_builder("nonexistent_consumer/nonexistent_resource");
  auto _purge_response = send_and_receive(_purge);

  ASSERT_EQ(_purge_response.size(), 1);

  ASSERT_EQ(static_cast<uint8_t>(_purge_response[0]), 0);
}

TEST_F(ServerTestFixture, HandlesStatReturnsCorrectMetrics)
{
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

  uint64_t _rpm = 0;
  uint64_t _wpm = 0;
  uint64_t _reads_total = 0;
  uint64_t _writes_total = 0;

  std::memcpy(&_rpm, _stat_response.data() + 1, sizeof(uint64_t));
  std::memcpy(&_wpm, _stat_response.data() + 1 + sizeof(uint64_t), sizeof(uint64_t));
  std::memcpy(&_reads_total, _stat_response.data() + 1 + sizeof(uint64_t) * 2, sizeof(uint64_t));
  std::memcpy(&_writes_total, _stat_response.data() + 1 + sizeof(uint64_t) * 3, sizeof(uint64_t));

  // Verificar que haya al menos 3 lecturas y 2 escrituras
  ASSERT_GE(_rpm, 3);
  ASSERT_GE(_wpm, 2);
  ASSERT_EQ(_reads_total, _rpm);
  ASSERT_EQ(_writes_total, _wpm);
}