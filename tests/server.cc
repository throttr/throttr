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
#include <boost/asio.hpp>
#include <thread>
#include <throttr/app.hpp>
#include <throttr/state.hpp>

using boost::asio::ip::tcp;
using namespace throttr;

class ServerTestFixture : public testing::Test {
    std::unique_ptr<std::jthread> server_thread_;
    std::shared_ptr<app> app_;
    int threads_ = 1;

protected:
    void SetUp() override {
        app_ = std::make_shared<app>(1337, threads_);

        server_thread_ = std::make_unique<std::jthread>([this]() {
            app_->serve();
        });

        while (!app_->state_->acceptor_ready_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    void TearDown() override {
        app_->stop();

        if (server_thread_ && server_thread_->joinable()) {
            server_thread_->join();
        }
    }

    [[nodiscard]] static std::vector<std::byte> send_and_receive(const std::vector<std::byte> &message, const int length = 1) {
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

TEST_F(ServerTestFixture, HandlesSingleValidRequest) {
    const auto _buffer = request_insert_builder(1, ttl_types::seconds, 32, "consumer1/resource1");
    auto _response = send_and_receive(_buffer);

    ASSERT_EQ(_response.size(), 1);
    ASSERT_EQ(static_cast<uint8_t>(_response[0]), 1);
}

TEST_F(ServerTestFixture, HandlesMultipleSingleRequests) {
    const auto _buffer = request_insert_builder(1, ttl_types::seconds, 32, "consumer2/resource2");

    for (int _i = 0; _i < 5; ++_i) {
        auto _response = send_and_receive(_buffer);
        ASSERT_EQ(_response.size(), 1);

        if (_i == 0) {
            ASSERT_EQ(static_cast<uint8_t>(_response[0]), 1);
        } else {
            ASSERT_EQ(static_cast<uint8_t>(_response[0]), 0);
        }
    }
}

TEST_F(ServerTestFixture, HandlesTwoConcatenatedRequests) {
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

TEST_F(ServerTestFixture, HandleSetSimpleValue) {
    const std::string _key = "consumer/set_value";
    const std::vector _value = {
        std::byte{0xBE}, std::byte{0xEF}, std::byte{0xCA}, std::byte{0xFE}
    };

    const auto _buffer = request_set_builder(_value, ttl_types::seconds, 10, _key);

    auto _response = send_and_receive(_buffer);

    ASSERT_EQ(_response.size(), 1);
    ASSERT_EQ(static_cast<uint8_t>(_response[0]), 1);
}

TEST_F(ServerTestFixture, HandleGetReturnsCorrectValue) {
    const std::string _key = "consumer/get_test";
    const std::vector<std::byte> _value = {
        std::byte{0xBA}, std::byte{0xAD}, std::byte{0xF0}, std::byte{0x0D}
    };

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
    for (std::size_t i = 0; i < _value.size(); ++i) {
        ASSERT_EQ(_value_ptr[i], _value[i]);
    }
}

TEST_F(ServerTestFixture, TTLExpiration) {
    const auto _insert_buffer = request_insert_builder(32, ttl_types::milliseconds, 28, "consumer3/expire");

    auto _ignored = send_and_receive(_insert_buffer);
    boost::ignore_unused(_ignored);

    const auto _query_buffer = request_query_builder("consumer3/expire");
    auto _success_query_response = send_and_receive(_query_buffer, 2 + (sizeof(value_type) * 2));

    // [x] [o][o] [o] [o][o]
    ASSERT_EQ(static_cast<uint8_t>(_success_query_response[0]), 1);

    value_type _success_response_quota;

    // [o] [x][x] [o] [o][o]
    std::memcpy(&_success_response_quota, _success_query_response.data() + 1, sizeof(value_type)); // 2 bytes
    ASSERT_EQ(_success_response_quota, 32);

    // [o] [o][o] [x] [o][o]
    const auto _success_response_ttl_type = static_cast<uint8_t>(_success_query_response[sizeof(value_type) + 1]);
    ASSERT_EQ(_success_response_ttl_type, static_cast<uint8_t>(ttl_types::milliseconds));

    value_type _success_response_ttl;

    // [o] [o][o] [o] [x][x]
    std::memcpy(&_success_response_ttl, _success_query_response.data() + sizeof(value_type) + 2, sizeof(value_type));
    ASSERT_GT(_success_response_ttl, 0);;

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    auto _expired_query_response = send_and_receive(_query_buffer);

    ASSERT_EQ(static_cast<uint8_t>(_expired_query_response[0]), 0);
}

TEST_F(ServerTestFixture, SeparateStocksForDifferentIDs) {
    const auto _buffer_a = request_insert_builder(3, ttl_types::seconds, 7, "consumerA/resourceA");
    const auto _buffer_b = request_insert_builder(5, ttl_types::seconds, 7, "consumerB/resourceB");

    const auto _response_a1 = send_and_receive(_buffer_a);
    ASSERT_EQ(_response_a1.size(), 1);
    ASSERT_EQ(static_cast<uint8_t>(_response_a1[0]), 1);

    const auto _response_b1 = send_and_receive(_buffer_b);
    ASSERT_EQ(_response_b1.size(), 1);
    ASSERT_EQ(static_cast<uint8_t>(_response_b1[0]), 1);
}

TEST_F(ServerTestFixture, QueryBeforeInsertReturnsZeroQuota) {
    const auto _buffer = request_query_builder("consumer_query/resource_query");
    auto _response = send_and_receive(_buffer);

    ASSERT_EQ(_response.size(), 1);
    ASSERT_EQ(static_cast<uint8_t>(_response[0]), 0);
}

TEST_F(ServerTestFixture, QueryAfterInsertReturnsCorrectQuota) {
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

TEST_F(ServerTestFixture, QueryExpiredReturnsZeroQuota) {
    const auto _insert_buffer = request_insert_builder(0, ttl_types::seconds, 1, "consumer_query3/resource_query3");

    auto _ignored = send_and_receive(_insert_buffer);
    boost::ignore_unused(_ignored);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    const auto _query_buffer = request_query_builder("consumer_query3/resource_query3");

    auto _response = send_and_receive(_query_buffer);
    ASSERT_EQ(static_cast<uint8_t>(_response[0]), 0);
}

TEST_F(ServerTestFixture, UpdateIncreaseQuota) {
    const auto _insert_buffer = request_insert_builder(0, ttl_types::seconds, 77, "consumer/increase_quota");

    auto _ignored = send_and_receive(_insert_buffer);
    boost::ignore_unused(_ignored);

    const auto _update_buffer = request_update_builder(attribute_types::quota, change_types::increase, 10, "consumer/increase_quota");
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

TEST_F(ServerTestFixture, UpdateDecreaseQuota) {
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

TEST_F(ServerTestFixture, UpdatePatchQuota) {
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

TEST_F(ServerTestFixture, UpdatePatchTTL) {
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

    const auto _ttl_type = static_cast<uint8_t>(_query_response[1+sizeof(_quota)]);
    ASSERT_EQ(_ttl_type, static_cast<uint8_t>(ttl_types::milliseconds));

    value_type _ttl = 0;
    std::memcpy(&_ttl, _query_response.data() + 2 + sizeof(_quota), sizeof(_ttl));
    ASSERT_GE(_ttl, 0);
    ASSERT_LE(_ttl, 64);
}

TEST_F(ServerTestFixture, UpdateNonExistentKeyReturnsError) {
    const auto _update = request_update_builder(attribute_types::quota, change_types::patch, 100, "non_existing_resource");

    auto _update_response = send_and_receive(_update);
    ASSERT_EQ(_update_response.size(), 1);

    ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 0);
}

TEST_F(ServerTestFixture, UpdateDecreaseQuotaBeyondZero) {
    const auto _insert = request_insert_builder(0, ttl_types::seconds, 32, "consumer_beyond/resource_beyond");

    auto _ignored = send_and_receive(_insert);
    boost::ignore_unused(_ignored);

    const auto _update = request_update_builder(attribute_types::quota, change_types::decrease, 10, "consumer_beyond/resource_beyond");
    auto _update_response = send_and_receive(_update);
    ASSERT_EQ(_update_response.size(), 1);
    ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 0);
}

TEST_F(ServerTestFixture, UpdatePatchTTLSeconds) {
    const auto _insert = request_insert_builder(0, ttl_types::seconds, 5, "consumer_sec/resource_sec");

    auto _ignored = send_and_receive(_insert);
    boost::ignore_unused(_ignored);

    const auto _update = request_update_builder(attribute_types::ttl, change_types::patch, 10, "consumer_sec/resource_sec");
    std::vector<std::byte> _update_response = send_and_receive(_update);
    ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);
}

TEST_F(ServerTestFixture, UpdateIncreaseTTL) {
    const auto _insert = request_insert_builder(0, ttl_types::milliseconds, 32, "consumer_inc/resource_inc");
    auto _ignored = send_and_receive(_insert);
    boost::ignore_unused(_ignored);

    const auto _update = request_update_builder(attribute_types::ttl, change_types::increase, 18, "consumer_inc/resource_inc");
    auto _update_response = send_and_receive(_update);

    ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);
}

TEST_F(ServerTestFixture, UpdateDecreaseTTL) {
    const auto _insert = request_insert_builder(0, ttl_types::milliseconds, 32, "consumer_dec/resource_dec");
    auto _ignored = send_and_receive(_insert);
    boost::ignore_unused(_ignored);

    const auto _update = request_update_builder(attribute_types::ttl, change_types::decrease, 12, "consumer_dec/resource_dec");
    auto _update_response = send_and_receive(_update);

    ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);
}

TEST_F(ServerTestFixture, PurgeExistingEntry) {
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

TEST_F(ServerTestFixture, PurgeNonExistentEntryReturnsError) {
    const auto _purge = request_purge_builder("nonexistent_consumer/nonexistent_resource");
    auto _purge_response = send_and_receive(_purge);

    ASSERT_EQ(_purge_response.size(), 1);

    ASSERT_EQ(static_cast<uint8_t>(_purge_response[0]), 0);
}
