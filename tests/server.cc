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
#include <iomanip>
#include <iostream>

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

    static void print_hex(const std::vector<std::byte> &data, const std::string &label) {
        std::cout << label << ": ";
        for (const auto &_byte: data) {
            std::cout << std::hex << std::setfill('0') << std::setw(2)
                    << static_cast<int>(std::to_integer<uint8_t>(_byte)) << " ";
        }
        std::cout << std::dec << std::endl;
    }

    [[nodiscard]] static std::vector<std::byte> send_and_receive(const std::vector<std::byte> &message,
                                                                 const int length = 5) {
        boost::asio::io_context _io_context;
        tcp::resolver _resolver(_io_context);

        print_hex(message, "Request");

        const auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(1337));

        tcp::socket _socket(_io_context);
        boost::asio::connect(_socket, _endpoints);

        boost::asio::write(_socket, boost::asio::buffer(message.data(), message.size()));

        std::vector<std::byte> _response(length);
        boost::asio::read(_socket, boost::asio::buffer(_response.data(), _response.size()));

        print_hex(_response, "Response");

        return _response;
    }
};

TEST_F(ServerTestFixture, HandlesSingleValidRequest) {
    const auto _buffer = request_insert_builder(
        100, 1, ttl_types::milliseconds, 10000, "consumer1", "/resource1"
    );

    auto _response = send_and_receive(_buffer);

    ASSERT_EQ(_response.size(), 5);
    uint32_t _value;
    std::memcpy(&_value, _response.data(), sizeof(uint32_t));
    ASSERT_EQ(_value, 100);
    ASSERT_EQ(static_cast<uint8_t>(_response[sizeof(uint32_t)]), 1);
}

TEST_F(ServerTestFixture, HandlesMultipleSingleRequests) {
    const auto _buffer = request_insert_builder(
        200, 1, ttl_types::milliseconds, 5000, "consumer2", "/resource2"
    );

    for (int _i = 0; _i < 5; ++_i) {
        auto _response = send_and_receive(_buffer);
        ASSERT_EQ(_response.size(), 5);

        uint32_t _request_id = 0;
        std::memcpy(&_request_id, _response.data(), sizeof(_request_id));
        ASSERT_EQ(_request_id, 200);

        if (_i == 0) {
            ASSERT_EQ(static_cast<uint8_t>(_response[sizeof(_request_id)]), 1);
        } else {
            ASSERT_EQ(static_cast<uint8_t>(_response[sizeof(_request_id)]), 0);
        }
    }
}

TEST_F(ServerTestFixture, TTLExpiration) {
    const auto _insert_buffer = request_insert_builder(
        300, 1, ttl_types::milliseconds, 1000, "consumer3", "/expire"
    );

    auto _ignored = send_and_receive(_insert_buffer);
    boost::ignore_unused(_ignored);

    const auto _query_buffer = request_query_builder(
        300, "consumer3", "/expire"
    );

    auto _success_query_response = send_and_receive(_query_buffer, 22);

    uint32_t _success_response_id;
    std::memcpy(&_success_response_id, _success_query_response.data(), sizeof(uint32_t));
    ASSERT_EQ(_success_response_id, 300);

    ASSERT_EQ(static_cast<uint8_t>(_success_query_response[4]), 1);

    uint64_t _success_response_quota;
    std::memcpy(&_success_response_quota, _success_query_response.data() + 5, sizeof(uint64_t));
    ASSERT_EQ(_success_response_quota, 1);

    const auto _success_response_ttl_type = static_cast<uint8_t>(_success_query_response[13]);
    ASSERT_EQ(_success_response_ttl_type, static_cast<uint8_t>(ttl_types::milliseconds));

    uint64_t _success_response_ttl;
    std::memcpy(&_success_response_ttl, _success_query_response.data() + 14, sizeof(uint64_t));
    ASSERT_GT(_success_response_ttl, 0);;

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    auto _expired_query_response = send_and_receive(_query_buffer);

    ASSERT_EQ(_expired_query_response.size(), 5);

    uint32_t _expired_response_id;
    std::memcpy(&_expired_response_id, _expired_query_response.data(), sizeof(uint32_t));
    ASSERT_EQ(_expired_response_id, 300);

    ASSERT_EQ(static_cast<uint8_t>(_expired_query_response[4]), 0);
}

TEST_F(ServerTestFixture, SeparateStocksForDifferentIDs) {
    const auto _buffer_a = request_insert_builder(
        400, 1, ttl_types::milliseconds, 5000, "consumerA", "/resourceA"
    );

    const auto _buffer_b = request_insert_builder(
        401, 1, ttl_types::milliseconds, 5000, "consumerB", "/resourceB"
    );

    auto _response_a1 = send_and_receive(_buffer_a);
    ASSERT_EQ(_response_a1.size(), 5);

    uint32_t _id_a;
    std::memcpy(&_id_a, _response_a1.data(), sizeof(_id_a));
    ASSERT_EQ(_id_a, 400);
    ASSERT_EQ(static_cast<uint8_t>(_response_a1[sizeof(_id_a)]), 1);

    auto _response_b1 = send_and_receive(_buffer_b);
    ASSERT_EQ(_response_b1.size(), 5);

    uint32_t _id_b;
    std::memcpy(&_id_b, _response_b1.data(), sizeof(_id_b));
    ASSERT_EQ(_id_b, 401);
    ASSERT_EQ(static_cast<uint8_t>(_response_b1[sizeof(_id_b)]), 1);
}

TEST_F(ServerTestFixture, QueryBeforeInsertReturnsZeroQuota) {
    const auto _buffer = request_query_builder(
        500, "consumer_query", "/resource_query"
    );

    auto _response = send_and_receive(_buffer, 5);

    ASSERT_EQ(_response.size(), 5);

    uint32_t _request_id = 0;
    std::memcpy(&_request_id, _response.data(), sizeof(_request_id));
    ASSERT_EQ(_request_id, 500);

    ASSERT_EQ(static_cast<uint8_t>(_response[sizeof(_request_id)]), 0);
}

TEST_F(ServerTestFixture, QueryAfterInsertReturnsCorrectQuota) {
    const auto _insert_buffer = request_insert_builder(
        600, 10, ttl_types::milliseconds, 10000, "consumer_query2", "/resource_query2"
    );

    auto _ignored = send_and_receive(_insert_buffer);
    boost::ignore_unused(_ignored);

    const auto _query_buffer = request_query_builder(
        601, "consumer_query2", "/resource_query2"
    );

    auto _response = send_and_receive(_query_buffer, 22);

    uint32_t _response_id = 0;
    std::memcpy(&_response_id, _response.data(), sizeof(_response_id));
    ASSERT_EQ(_response_id, 601);

    ASSERT_EQ(static_cast<uint8_t>(_response[4]), 1);

    uint64_t _quota_remaining = 0;
    std::memcpy(&_quota_remaining, _response.data() + 5, sizeof(_quota_remaining));
    ASSERT_EQ(_quota_remaining, 10);
}

TEST_F(ServerTestFixture, QueryExpiredReturnsZeroQuota) {
    const auto _insert_buffer = request_insert_builder(
        700, 0, ttl_types::milliseconds, 500, "consumer_query3", "/resource_query3"
    );

    auto _ignored = send_and_receive(_insert_buffer);
    boost::ignore_unused(_ignored);

    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    const auto _query_buffer = request_query_builder(
        701, "consumer_query3", "/resource_query3"
    );

    auto _response = send_and_receive(_query_buffer, 5);

    uint32_t _response_id = 0;
    std::memcpy(&_response_id, _response.data(), sizeof(_response_id));
    ASSERT_EQ(_response_id, 701);

    ASSERT_EQ(static_cast<uint8_t>(_response[4]), 0);
}

TEST_F(ServerTestFixture, UpdateAfterInsertModifiesQuota) {
    const auto _insert_buffer = request_insert_builder(
        800, 0, ttl_types::milliseconds, 5000, "consumer_update", "/resource_update"
    );

    auto _ignored = send_and_receive(_insert_buffer);
    boost::ignore_unused(_ignored);

    const auto _update_buffer = request_update_builder(
        801, attribute_types::quota, change_types::increase, 10,
        "consumer_update", "/resource_update"
    );
    auto _update_response = send_and_receive(_update_buffer, 5);

    ASSERT_EQ(_update_response.size(), 5);

    uint32_t _update_response_id = 0;
    std::memcpy(&_update_response_id, _update_response.data(), sizeof(uint32_t));
    ASSERT_EQ(_update_response_id, 801);

    ASSERT_EQ(static_cast<uint8_t>(_update_response[4]), 1);

    const auto _query_buffer = request_query_builder(
        802, "consumer_update", "/resource_update"
    );
    auto _query_response = send_and_receive(_query_buffer, 22);

    ASSERT_EQ(_query_response.size(), 22);

    uint32_t _query_response_id = 0;
    std::memcpy(&_query_response_id, _query_response.data(), sizeof(uint32_t));
    ASSERT_EQ(_query_response_id, 802);

    ASSERT_EQ(static_cast<uint8_t>(_query_response[4]), 1);

    uint64_t _quota = 0;
    std::memcpy(&_quota, _query_response.data() + 5, sizeof(uint64_t));
    ASSERT_EQ(_quota, 10);
}

TEST_F(ServerTestFixture, UpdateIncreaseQuota) {
    const auto _insert = request_insert_builder(
        900, 5, ttl_types::milliseconds, 10000, "consumer_increase", "/resource_increase"
    );

    auto _ignored = send_and_receive(_insert);
    boost::ignore_unused(_ignored);

    const auto _update = request_update_builder(
        901, attribute_types::quota, change_types::increase, 10, "consumer_increase", "/resource_increase"
    );

    auto _update_response = send_and_receive(_update, 5);
    ASSERT_EQ(_update_response.size(), 5);

    uint32_t _update_response_id = 0;
    std::memcpy(&_update_response_id, _update_response.data(), sizeof(uint32_t));
    ASSERT_EQ(_update_response_id, 901);

    ASSERT_EQ(static_cast<uint8_t>(_update_response[4]), 1);

    const auto _query = request_query_builder(902, "consumer_increase", "/resource_increase");
    const auto _query_response = send_and_receive(_query, 22);

    ASSERT_EQ(_query_response.size(), 22);

    uint64_t _quota_remaining = 0;
    std::memcpy(&_quota_remaining, _query_response.data() + 5, sizeof(_quota_remaining));
    ASSERT_EQ(_quota_remaining, 15);
}

TEST_F(ServerTestFixture, UpdateDecreaseQuota) {
    const auto _insert = request_insert_builder(
        1000, 10, ttl_types::milliseconds, 10000, "consumer_decrease", "/resource_decrease"
    );

    auto _ignored = send_and_receive(_insert);
    boost::ignore_unused(_ignored);

    const auto _update = request_update_builder(
        1001, attribute_types::quota, change_types::decrease, 4, "consumer_decrease", "/resource_decrease"
    );

    auto _update_response = send_and_receive(_update, 5);
    ASSERT_EQ(_update_response.size(), 5);

    uint32_t _update_response_id = 0;
    std::memcpy(&_update_response_id, _update_response.data(), sizeof(uint32_t));
    ASSERT_EQ(_update_response_id, 1001);

    ASSERT_EQ(static_cast<uint8_t>(_update_response[4]), 1);

    const auto _query = request_query_builder(1002, "consumer_decrease", "/resource_decrease");
    const auto _query_response = send_and_receive(_query, 22);

    uint64_t _quota_remaining = 0;
    std::memcpy(&_quota_remaining, _query_response.data() + 5, sizeof(_quota_remaining));
    ASSERT_EQ(_quota_remaining, 6);
}

TEST_F(ServerTestFixture, UpdatePatchTTL) {
    const auto _insert = request_insert_builder(
        1100, 10, ttl_types::milliseconds, 5000, "consumer_patchttl", "/resource_patchttl"
    );

    auto _ignored = send_and_receive(_insert);
    boost::ignore_unused(_ignored);

    const auto _update = request_update_builder(
        1101, attribute_types::ttl, change_types::patch, 10000, "consumer_patchttl", "/resource_patchttl"
    );
    auto _update_response = send_and_receive(_update, 5);
    ASSERT_EQ(_update_response.size(), 5);

    uint32_t _update_response_id = 0;
    std::memcpy(&_update_response_id, _update_response.data(), sizeof(uint32_t));
    ASSERT_EQ(_update_response_id, 1101);
    ASSERT_EQ(static_cast<uint8_t>(_update_response[4]), 1);

    const auto _query = request_query_builder(1102, "consumer_patchttl", "/resource_patchttl");
    auto _query_response = send_and_receive(_query, 22);

    ASSERT_EQ(_query_response.size(), 22);

    uint32_t _query_response_id = 0;
    std::memcpy(&_query_response_id, _query_response.data(), sizeof(uint32_t));
    ASSERT_EQ(_query_response_id, 1102);
    ASSERT_EQ(static_cast<uint8_t>(_query_response[4]), 1);

    uint64_t _quota_remaining = 0;
    std::memcpy(&_quota_remaining, _query_response.data() + 5, sizeof(_quota_remaining));
    ASSERT_EQ(_quota_remaining, 10);

    const auto _ttl_type = static_cast<uint8_t>(_query_response[13]);
    ASSERT_EQ(_ttl_type, static_cast<uint8_t>(ttl_types::milliseconds));

    uint64_t _ttl_remaining = 0;
    std::memcpy(&_ttl_remaining, _query_response.data() + 14, sizeof(uint64_t));
    ASSERT_GE(_ttl_remaining, 9000);
    ASSERT_LE(_ttl_remaining, 10000);
}

TEST_F(ServerTestFixture, UpdateNonExistentKeyReturnsError) {
    const auto _update = request_update_builder(
        1200, attribute_types::quota, change_types::patch, 100, "nonexistent_consumer", "/nonexistent_resource"
    );

    auto _update_response = send_and_receive(_update, 5);
    ASSERT_EQ(_update_response.size(), 5);

    uint32_t _response_id = 0;
    std::memcpy(&_response_id, _update_response.data(), sizeof(uint32_t));
    ASSERT_EQ(_response_id, 1200);

    ASSERT_EQ(static_cast<uint8_t>(_update_response[4]), 0);
}

TEST_F(ServerTestFixture, UpdateDecreaseQuotaBeyondZero) {
    const auto _insert = request_insert_builder(1300, 0, ttl_types::milliseconds, 10000, "consumer_beyond",
                                                "/resource_beyond");

    auto _ignored = send_and_receive(_insert);
    boost::ignore_unused(_ignored);

    const auto _update = request_update_builder(
        1300, attribute_types::quota, change_types::decrease, 10, "consumer_beyond", "/resource_beyond"
    );

    auto _update_response = send_and_receive(_update, 5);

    ASSERT_EQ(_update_response.size(), 5);

    uint32_t _response_id = 0;
    std::memcpy(&_response_id, _update_response.data(), sizeof(uint32_t));
    ASSERT_EQ(_response_id, 1300);

    ASSERT_EQ(static_cast<uint8_t>(_update_response[4]), 1);

    const auto _query = request_query_builder(1301, "consumer_beyond", "/resource_beyond");
    const auto _query_response = send_and_receive(_query, 22);

    uint64_t _quota_remaining = 0;
    std::memcpy(&_quota_remaining, _query_response.data() + 5, sizeof(_quota_remaining));
    ASSERT_EQ(_quota_remaining, 0);
}

TEST_F(ServerTestFixture, UpdatePatchTTLSeconds) {
    const auto _insert = request_insert_builder(
        1400, 0, ttl_types::seconds, 5, "consumer_sec", "/resource_sec"
    );

    auto _ignored = send_and_receive(_insert);
    boost::ignore_unused(_ignored);

    const auto _update = request_update_builder(
        1401, attribute_types::ttl, change_types::patch, 10, "consumer_sec", "/resource_sec"
    );

    std::vector<std::byte> _update_response = send_and_receive(_update);

    uint32_t _response_id = 0;
    std::memcpy(&_response_id, _update_response.data(), sizeof(uint32_t));
    ASSERT_EQ(_response_id, 1401);

    ASSERT_EQ(static_cast<uint8_t>(_update_response[4]), 1);
}

TEST_F(ServerTestFixture, UpdatePatchTTLNanoseconds) {
    const auto _insert = request_insert_builder(
        1500, 0, ttl_types::nanoseconds, 5'000'000'000, "consumer_ns", "/resource_ns"
    );

    auto _ignored = send_and_receive(_insert);
    boost::ignore_unused(_ignored);

    const auto _update = request_update_builder(
        1501, attribute_types::ttl, change_types::patch, 7'000'000'000, "consumer_ns", "/resource_ns"
    );

    std::vector<std::byte> _update_response = send_and_receive(_update);

    uint32_t _response_id = 0;
    std::memcpy(&_response_id, _update_response.data(), sizeof(uint32_t));
    ASSERT_EQ(_response_id, 1501);

    ASSERT_EQ(static_cast<uint8_t>(_update_response[4]), 1);
}

TEST_F(ServerTestFixture, UpdateIncreaseTTL) {
    const auto _insert = request_insert_builder(
        1600, 0, ttl_types::milliseconds, 5000, "consumer_inc", "/resource_inc"
    );

    auto _ignored = send_and_receive(_insert);
    boost::ignore_unused(_ignored);

    const auto _update = request_update_builder(
        1601, attribute_types::ttl, change_types::increase, 3000, "consumer_inc", "/resource_inc"
    );

    auto _update_response = send_and_receive(_update);

    uint32_t _response_id = 0;
    std::memcpy(&_response_id, _update_response.data(), sizeof(uint32_t));
    ASSERT_EQ(_response_id, 1601);

    ASSERT_EQ(static_cast<uint8_t>(_update_response[4]), 1);
}

TEST_F(ServerTestFixture, UpdateDecreaseTTL) {
    const auto _insert = request_insert_builder(
        1700, 0, ttl_types::milliseconds, 5000, "consumer_dec", "/resource_dec"
    );

    auto _ignored = send_and_receive(_insert);
    boost::ignore_unused(_ignored);

    const auto _update = request_update_builder(
        1701, attribute_types::ttl, change_types::decrease, 3000, "consumer_dec", "/resource_dec"
    );

    auto _update_response = send_and_receive(_update);

    uint32_t _response_id = 0;
    std::memcpy(&_response_id, _update_response.data(), sizeof(uint32_t));
    ASSERT_EQ(_response_id, 1701);

    ASSERT_EQ(static_cast<uint8_t>(_update_response[4]), 1);
}

TEST_F(ServerTestFixture, PurgeExistingEntry) {
    const auto _insert = request_insert_builder(
        1800, 1, ttl_types::milliseconds, 100'000'000, "consumer_purge", "/resource_purge"
    );

    auto _ignored = send_and_receive(_insert);
    boost::ignore_unused(_ignored);

    const auto _purge = request_purge_builder(1801, "consumer_purge", "/resource_purge");
    auto _purge_response = send_and_receive(_purge);

    ASSERT_EQ(_purge_response.size(), 5);

    uint32_t _purge_id = 0;
    std::memcpy(&_purge_id, _purge_response.data(), sizeof(_purge_id));
    ASSERT_EQ(_purge_id, 1801);

    ASSERT_EQ(static_cast<uint8_t>(_purge_response[4]), 1);

    const auto _query = request_query_builder(1802, "consumer_purge", "/resource_purge");
    const auto _query_response = send_and_receive(_query, 5);

    ASSERT_EQ(_query_response.size(), 5);

    uint32_t _query_id = 0;
    std::memcpy(&_query_id, _query_response.data(), sizeof(_query_id));
    ASSERT_EQ(_query_id, 1802);

    ASSERT_EQ(static_cast<uint8_t>(_query_response[4]), 0);
}

TEST_F(ServerTestFixture, PurgeNonExistentEntryReturnsError) {
    const auto _purge = request_purge_builder(1900, "nonexistent_consumer", "/nonexistent_resource");
    auto _purge_response = send_and_receive(_purge);

    ASSERT_EQ(_purge_response.size(), 5);

    uint32_t _purge_id = 0;
    std::memcpy(&_purge_id, _purge_response.data(), sizeof(_purge_id));
    ASSERT_EQ(_purge_id, 1900);

    ASSERT_EQ(static_cast<uint8_t>(_purge_response[4]), 0);
}
