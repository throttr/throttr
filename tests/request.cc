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
#include <throttr/request.hpp>
#include <chrono>
#include <thread>
#include <vector>

using namespace throttr;
using namespace std::chrono;

TEST(RequestInsertTest, ParseAndSerialize) {
    auto _buffer = request_insert_builder(
        5000, 5, ttl_types::milliseconds, 60000, "consumer123", "/api/resource"
    );

    const auto _request = request_insert::from_buffer(_buffer);
    EXPECT_EQ(_request.header_->quota_, 5000);
    EXPECT_EQ(_request.header_->usage_, 5);
    EXPECT_EQ(_request.header_->ttl_type_, ttl_types::milliseconds);
    EXPECT_EQ(_request.header_->ttl_, 60000);
    EXPECT_EQ(_request.consumer_id_, "consumer123");
    EXPECT_EQ(_request.resource_id_, "/api/resource");

    auto _reconstructed = _request.to_buffer();
    ASSERT_EQ(_reconstructed.size(), _buffer.size());
    ASSERT_TRUE(std::equal(_reconstructed.begin(), _reconstructed.end(), _buffer.begin()));
}

TEST(RequestQueryTest, ParseAndSerialize) {
    auto _buffer = request_query_builder("consumerABC", "/resourceXYZ");

    const auto _request = request_query::from_buffer(_buffer);
    EXPECT_EQ(_request.consumer_id_, "consumerABC");
    EXPECT_EQ(_request.resource_id_, "/resourceXYZ");

    auto _reconstructed = _request.to_buffer();
    ASSERT_EQ(_reconstructed.size(), _buffer.size());
    ASSERT_TRUE(std::equal(_reconstructed.begin(), _reconstructed.end(), _buffer.begin()));
}

TEST(RequestInsertTest, RejectsTooSmallBuffer) {
    std::vector _buffer(5, static_cast<std::byte>(0));
    ASSERT_THROW(request_insert::from_buffer(_buffer), request_error);
}

TEST(RequestQueryTest, RejectsTooSmallBuffer) {
    std::vector _buffer(2, static_cast<std::byte>(0));
    ASSERT_THROW(request_query::from_buffer(_buffer), request_error);
}

TEST(RequestInsertBenchmark, DecodePerformance) {
    auto _buffer = request_insert_builder(
        5000, 5, ttl_types::milliseconds, 60000, "consumer123", "/api/benchmark"
    );

    constexpr size_t _iterations = 1'000'000;
    const auto _start = high_resolution_clock::now();

    for (size_t _i = 0; _i < _iterations; ++_i) {
        const auto _view = request_insert::from_buffer(_buffer);
        EXPECT_EQ(_view.header_->quota_, 5000);
    }

    const auto _end = high_resolution_clock::now();
    const auto _duration = duration_cast<milliseconds>(_end - _start);

    std::cout << "RequestInsert iterations: " << _iterations
              << " on " << _duration.count() << " ms" << std::endl;
}

TEST(RequestQueryBenchmark, DecodePerformance) {
    auto _buffer = request_query_builder(
        "consumerABC", "/api/query"
    );

    constexpr size_t _iterations = 1'000'000;
    const auto _start = high_resolution_clock::now();

    for (size_t _i = 0; _i < _iterations; ++_i) {
        auto _view = request_query::from_buffer(_buffer);
        EXPECT_EQ(_view.consumer_id_, "consumerABC");
    }

    const auto _end = high_resolution_clock::now();
    const auto _duration = duration_cast<milliseconds>(_end - _start);

    std::cout << "RequestQuery iterations: " << _iterations
              << " on " << _duration.count() << " ms" << std::endl;
}

TEST(RequestInsertTest, RejectsInvalidPayloadSize) {
    std::vector<std::byte> _buffer(request_insert_header_size + 5);

    auto *_header = reinterpret_cast<request_insert_header*>(_buffer.data()); // NOSONAR
    _header->request_type_ = request_types::insert;
    _header->quota_ = 10;
    _header->usage_ = 0;
    _header->ttl_type_ = ttl_types::milliseconds;
    _header->ttl_ = 10000;
    _header->consumer_id_size_ = 5;
    _header->resource_id_size_ = 5;

    ASSERT_THROW(request_insert::from_buffer(_buffer), request_error);
}

TEST(RequestQueryTest, RejectsInvalidPayloadSize) {
    std::vector<std::byte> _buffer(request_query_header_size + 5);

    auto *_header = reinterpret_cast<request_query_header*>(_buffer.data()); // NOSONAR
    _header->request_type_ = request_types::query;
    _header->consumer_id_size_ = 5;
    _header->resource_id_size_ = 5;

    ASSERT_THROW(request_query::from_buffer(_buffer), request_error);
}

TEST(RequestKeyTest, EqualsIdenticalKeys) {
    const request_key _a{
        "consumer1",
        "/resource1"
    };

    const request_key _b{
        "consumer1",
        "/resource1"
    };

    EXPECT_TRUE(_a == _b);
}

TEST(RequestKeyTest, NotEqualsDifferentConsumerID) {
    const request_key _a{
        "consumer1",
        "/resource1"
    };

    const request_key _b{
        "consumer2",
        "/resource1"
    };

    EXPECT_FALSE(_a == _b);
}

TEST(RequestKeyTest, NotEqualsDifferentResourceID) {
    const request_key _a{
        "consumer1",
        "/resource1"
    };

    const request_key _b{
        "consumer1",
        "/resource2"
    };

    EXPECT_FALSE(_a == _b);
}

TEST(RequestKeyTest, NotEqualsDifferentBothFields) {
    const request_key _a{
        "consumer1",
        "/resource1"
    };

    const request_key _b{
        "consumer2",
        "/resource2"
    };

    EXPECT_FALSE(_a == _b);
}
