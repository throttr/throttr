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

    [[nodiscard]] static std::vector<std::byte> send_and_receive(const std::vector<std::byte>& message, int length = 18) {
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

    [[nodiscard]] static std::vector<std::byte> send_and_receive_corrupt(const std::vector<std::byte>& message) {
        boost::asio::io_context _io_context;
        tcp::resolver _resolver(_io_context);

        const auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(1337));

        tcp::socket _socket(_io_context);
        boost::asio::connect(_socket, _endpoints);

        boost::asio::write(_socket, boost::asio::buffer(message.data(), message.size()));

        std::vector<std::byte> _response(1);
        boost::asio::read(_socket, boost::asio::buffer(_response.data(), _response.size()));

        return _response;
    }
};

TEST_F(ServerTestFixture, HandlesSingleValidRequest) {
    const auto _buffer = request_insert_builder(
        5, 1, ttl_types::milliseconds, 10000, "consumer1", "/resource1"
    );

    auto _response = send_and_receive(_buffer);

    ASSERT_EQ(_response.size(), 18);
    ASSERT_EQ(static_cast<uint8_t>(_response[0]), 1);

    uint64_t _quota_remaining = 0;
    std::memcpy(&_quota_remaining, _response.data() + 1, sizeof(_quota_remaining));
    ASSERT_EQ(_quota_remaining, 4);
}

TEST_F(ServerTestFixture, HandlesMultipleValidRequests) {
    const auto _buffer = request_insert_builder(
        3, 1, ttl_types::milliseconds, 5000, "consumer2", "/resource2"
    );

    auto _response1 = send_and_receive(_buffer);
    ASSERT_EQ(static_cast<uint8_t>(_response1[0]), 1);

    uint64_t _available1 = 0;
    std::memcpy(&_available1, _response1.data() + 1, sizeof(_available1));
    ASSERT_EQ(_available1, 2);

    auto _response2 = send_and_receive(_buffer);
    ASSERT_EQ(static_cast<uint8_t>(_response2[0]), 1);

    uint64_t _available2 = 0;
    std::memcpy(&_available2, _response2.data() + 1, sizeof(_available2));
    ASSERT_EQ(_available2, 1);

    auto _response3 = send_and_receive(_buffer);
    ASSERT_EQ(static_cast<uint8_t>(_response3[0]), 1);

    uint64_t _available3 = 0;
    std::memcpy(&_available3, _response3.data() + 1, sizeof(_available3));
    ASSERT_EQ(_available3, 0);

    auto _response4 = send_and_receive(_buffer);
    ASSERT_EQ(static_cast<uint8_t>(_response4[0]), 0);
}

TEST_F(ServerTestFixture, TTLExpiration) {
    const auto _buffer = request_insert_builder(
        10, 1, ttl_types::milliseconds, 1000, "consumer3", "/expire"
    );

    auto _response1 = send_and_receive(_buffer);
    ASSERT_TRUE(static_cast<uint8_t>(_response1[0]));

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    auto _response2 = send_and_receive(_buffer);
    ASSERT_TRUE(static_cast<uint8_t>(_response2[0]));

    uint64_t _available = 0;
    std::memcpy(&_available, _response2.data() + 1, sizeof(_available));
    ASSERT_EQ(_available, 9);
}

TEST_F(ServerTestFixture, SeparateStocksForDifferentIDs) {
    const auto _buffer_a = request_insert_builder(
        2, 1, ttl_types::milliseconds, 5000, "consumerA", "/resourceA"
    );

    const auto _buffer_b = request_insert_builder(
        2, 1, ttl_types::milliseconds, 5000, "consumerB", "/resourceB"
    );

    auto _response_a1 = send_and_receive(_buffer_a);
    ASSERT_TRUE(static_cast<uint8_t>(_response_a1[0]));

    auto _response_b1 = send_and_receive(_buffer_b);
    ASSERT_TRUE(static_cast<uint8_t>(_response_b1[0]));

    uint64_t _available_a1 = 0;
    std::memcpy(&_available_a1, _response_a1.data() + 1, sizeof(_available_a1));
    ASSERT_EQ(_available_a1, 1);

    uint64_t _available_b1 = 0;
    std::memcpy(&_available_b1, _response_b1.data() + 1, sizeof(_available_b1));
    ASSERT_EQ(_available_b1, 1);
}

TEST_F(ServerTestFixture, HandlesCorruptRequestGracefully) {
    const std::vector<std::byte> _corrupt_buffer(5);

    const auto _response = send_and_receive_corrupt(_corrupt_buffer);
    ASSERT_EQ(_response.size(), 1);
    ASSERT_EQ(static_cast<uint8_t>(_response[0]), 0);
}


TEST_F(ServerTestFixture, QueryBeforeInsertReturnsZeroQuota) {
    const auto _buffer = request_query_builder("consumer_query", "/resource_query");

    auto _response = send_and_receive(_buffer);

    ASSERT_EQ(_response.size(), 18);
    ASSERT_EQ(static_cast<uint8_t>(_response[0]), 0);

    uint64_t _quota_remaining = 0;
    std::memcpy(&_quota_remaining, _response.data() + 1, sizeof(_quota_remaining));
    ASSERT_EQ(_quota_remaining, 0);
}

TEST_F(ServerTestFixture, QueryAfterInsertReturnsCorrectQuota) {
    {
        boost::asio::io_context _io_context;
        tcp::resolver _resolver(_io_context);
        const auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(1337));

        tcp::socket _socket(_io_context);
        boost::asio::connect(_socket, _endpoints);

        auto _insert = request_insert_builder(10, 0, ttl_types::milliseconds, 10000, "consumer_query2", "/resource_query2");
        boost::asio::write(_socket, boost::asio::buffer(_insert.data(), _insert.size()));

        std::vector<std::byte> _response(18);
        boost::asio::read(_socket, boost::asio::buffer(_response.data(), _response.size()));
    }

    const auto _query = request_query_builder("consumer_query2", "/resource_query2");
    auto _response = send_and_receive(_query);

    ASSERT_EQ(_response.size(), 18);
    ASSERT_EQ(static_cast<uint8_t>(_response[0]), 1);

    uint64_t _quota_remaining = 0;
    std::memcpy(&_quota_remaining, _response.data() + 1, sizeof(_quota_remaining));
    ASSERT_EQ(_quota_remaining, 10);
}

TEST_F(ServerTestFixture, QueryExpiredReturnsZeroQuota) {
    {
        boost::asio::io_context _io_context;
        tcp::resolver _resolver(_io_context);
        const auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(1337));

        tcp::socket _socket(_io_context);
        boost::asio::connect(_socket, _endpoints);

        auto _insert = request_insert_builder(5, 0, ttl_types::milliseconds, 500, "consumer_query3", "/resource_query3");
        boost::asio::write(_socket, boost::asio::buffer(_insert.data(), _insert.size()));

        std::vector<std::byte> _response(18);
        boost::asio::read(_socket, boost::asio::buffer(_response.data(), _response.size()));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    const auto _query = request_query_builder("consumer_query3", "/resource_query3");
    auto _response = send_and_receive(_query);

    ASSERT_EQ(_response.size(), 18);
    ASSERT_EQ(static_cast<uint8_t>(_response[0]), 0);

    uint64_t _quota_remaining = 0;
    std::memcpy(&_quota_remaining, _response.data() + 1, sizeof(_quota_remaining));
    ASSERT_EQ(_quota_remaining, 0);
}

TEST_F(ServerTestFixture, UpdatePatchQuota) {
    const auto _insert = request_insert_builder(5, 0, ttl_types::milliseconds, 10000, "consumer_patch", "/resource_patch");

    auto ignored = send_and_receive(_insert);
    boost::ignore_unused(ignored);

    const auto _update = request_update_builder(
        attribute_types::quota, change_types::patch, 20, "consumer_patch", "/resource_patch"
    );

    auto _update_response = send_and_receive(_update, 1);

    ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);

    const auto _query = request_query_builder("consumer_patch", "/resource_patch");
    auto _query_response = send_and_receive(_query);

    uint64_t _quota_remaining = 0;
    std::memcpy(&_quota_remaining, _query_response.data() + 1, sizeof(_quota_remaining));
    ASSERT_EQ(_quota_remaining, 20);
}

TEST_F(ServerTestFixture, UpdateIncreaseQuota) {
    const auto _insert = request_insert_builder(5, 0, ttl_types::milliseconds, 10000, "consumer_increase", "/resource_increase");
    auto ignored = send_and_receive(_insert);
    boost::ignore_unused(ignored);

    const auto _update = request_update_builder(
        attribute_types::quota, change_types::increase, 10, "consumer_increase", "/resource_increase"
    );

    auto _update_response = send_and_receive(_update, 1);
    ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);

    const auto _query = request_query_builder("consumer_increase", "/resource_increase");
    auto _query_response = send_and_receive(_query);

    uint64_t _quota_remaining = 0;
    std::memcpy(&_quota_remaining, _query_response.data() + 1, sizeof(_quota_remaining));
    ASSERT_EQ(_quota_remaining, 15);
}

TEST_F(ServerTestFixture, UpdateDecreaseQuota) {
    const auto _insert = request_insert_builder(10, 0, ttl_types::milliseconds, 10000, "consumer_decrease", "/resource_decrease");
    auto ignored = send_and_receive(_insert);
    boost::ignore_unused(ignored);

    const auto _update = request_update_builder(
        attribute_types::quota, change_types::decrease, 4, "consumer_decrease", "/resource_decrease"
    );

    auto _update_response = send_and_receive(_update, 1);
    ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);

    const auto _query = request_query_builder("consumer_decrease", "/resource_decrease");
    auto _query_response = send_and_receive(_query);

    uint64_t _quota_remaining = 0;
    std::memcpy(&_quota_remaining, _query_response.data() + 1, sizeof(_quota_remaining));
    ASSERT_EQ(_quota_remaining, 6);
}

TEST_F(ServerTestFixture, UpdatePatchTTL) {
    const auto _insert = request_insert_builder(10, 0, ttl_types::milliseconds, 5000, "consumer_patchttl", "/resource_patchttl");
    auto ignored = send_and_receive(_insert);
    boost::ignore_unused(ignored);

    const auto _update = request_update_builder(
        attribute_types::ttl, change_types::patch, 10000, "consumer_patchttl", "/resource_patchttl"
    );

    auto _update_response = send_and_receive(_update, 1);
    ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);

    const auto _query = request_query_builder("consumer_patchttl", "/resource_patchttl");
    auto _query_response = send_and_receive(_query);

    ASSERT_EQ(static_cast<uint8_t>(_query_response[0]), 1);

    uint64_t _quota_remaining = 0;
    std::memcpy(&_quota_remaining, _query_response.data() + 1, sizeof(_quota_remaining));
    ASSERT_EQ(_quota_remaining, 10);
}

TEST_F(ServerTestFixture, UpdateNonExistentKeyReturnsError) {
    const auto _update = request_update_builder(
        attribute_types::quota, change_types::patch, 100, "nonexistent_consumer", "/nonexistent_resource"
    );

    auto _update_response = send_and_receive(_update, 1);
    ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 0);
}

TEST_F(ServerTestFixture, UpdateDecreaseQuotaBeyondZero) {
    const auto _insert = request_insert_builder(5, 0, ttl_types::milliseconds, 10000, "consumer_beyond", "/resource_beyond");
    auto ignored_1 = send_and_receive(_insert);

    const auto _update = request_update_builder(
        attribute_types::quota, change_types::decrease, 10, "consumer_beyond", "/resource_beyond"
    );
    auto ignored_2 = send_and_receive(_update, 1);
    boost::ignore_unused(ignored_1, ignored_2);

    const auto _query = request_query_builder("consumer_beyond", "/resource_beyond");
    const auto _query_response = send_and_receive(_query);

    uint64_t _quota_remaining = 0;
    std::memcpy(&_quota_remaining, _query_response.data() + 1, sizeof(_quota_remaining));
    ASSERT_EQ(_quota_remaining, 0);
}

TEST_F(ServerTestFixture, UpdatePatchTTLSeconds) {
    const auto _insert = request_insert_builder(10, 0, ttl_types::seconds, 5, "consumer_sec", "/resource_sec");
    auto ignored = send_and_receive(_insert);
    boost::ignore_unused(ignored);

    const auto _update = request_update_builder(
        attribute_types::ttl, change_types::patch, 10, "consumer_sec", "/resource_sec"
    );
    auto _update_response = send_and_receive(_update, 1);
    ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);
}

TEST_F(ServerTestFixture, UpdatePatchTTLNanoseconds) {
    const auto _insert = request_insert_builder(10, 0, ttl_types::nanoseconds, 5000, "consumer_ns", "/resource_ns");
    auto ignored = send_and_receive(_insert);
    boost::ignore_unused(ignored);

    const auto _update = request_update_builder(
        attribute_types::ttl, change_types::patch, 10000, "consumer_ns", "/resource_ns"
    );
    auto _update_response = send_and_receive(_update, 1);
    ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);
}

TEST_F(ServerTestFixture, UpdateIncreaseTTL) {
    const auto _insert = request_insert_builder(10, 0, ttl_types::milliseconds, 5000, "consumer_inc", "/resource_inc");
    auto ignored = send_and_receive(_insert);
    boost::ignore_unused(ignored);

    const auto _update = request_update_builder(
        attribute_types::ttl, change_types::increase, 3000, "consumer_inc", "/resource_inc"
    );
    auto _update_response = send_and_receive(_update, 1);
    ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);
}

TEST_F(ServerTestFixture, UpdateDecreaseTTL) {
    const auto _insert = request_insert_builder(10, 0, ttl_types::milliseconds, 5000, "consumer_dec", "/resource_dec");
    auto ignored = send_and_receive(_insert);
    boost::ignore_unused(ignored);

    const auto _update = request_update_builder(
        attribute_types::ttl, change_types::decrease, 3000, "consumer_dec", "/resource_dec"
    );
    auto _update_response = send_and_receive(_update, 1);
    ASSERT_EQ(static_cast<uint8_t>(_update_response[0]), 1);
}

TEST_F(ServerTestFixture, PurgeExistingEntry) {
    const auto _insert = request_insert_builder(5, 1, ttl_types::milliseconds, 10000, "consumer_purge", "/resource_purge");
    auto ignored_insert = send_and_receive(_insert);
    boost::ignore_unused(ignored_insert);

    const auto _purge = request_purge_builder("consumer_purge", "/resource_purge");
    auto _purge_response = send_and_receive(_purge, 1);

    ASSERT_EQ(_purge_response.size(), 1);
    ASSERT_EQ(static_cast<uint8_t>(_purge_response[0]), 1);

    const auto _query = request_query_builder("consumer_purge", "/resource_purge");
    const auto _query_response = send_and_receive(_query);

    uint64_t _quota_remaining = 0;
    std::memcpy(&_quota_remaining, _query_response.data() + 1, sizeof(_quota_remaining));
    ASSERT_EQ(_quota_remaining, 0);
}

TEST_F(ServerTestFixture, PurgeNonExistentEntryReturnsError) {
    const auto _purge = request_purge_builder("nonexistent_consumer", "/nonexistent_resource");
    auto _purge_response = send_and_receive(_purge, 1);

    ASSERT_EQ(_purge_response.size(), 1);
    ASSERT_EQ(static_cast<uint8_t>(_purge_response[0]), 0);
}