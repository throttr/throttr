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
#include <throttr/logger.hpp>

using boost::asio::ip::tcp;

static std::vector<std::byte> build_request_buffer(
    const uint8_t ip_version,
    const std::array<uint8_t, 16>& ip,
    const uint16_t port,
    const uint32_t max_requests,
    const uint32_t ttl,
    const std::string_view url
) {
    std::vector<std::byte> buffer;
    buffer.resize(sizeof(throttr::request_header) + url.size());

    throttr::request_header header{};
    header.ip_version_ = ip_version;
    header.ip_ = ip;
    header.port_ = port;
    header.max_requests_ = max_requests;
    header.ttl_ = ttl;
    header.size_ = static_cast<uint8_t>(url.size());

    std::memcpy(buffer.data(), &header, sizeof(header));
    std::memcpy(buffer.data() + sizeof(throttr::request_header), url.data(), url.size());

    return buffer;
}

class ServerTestFixture : public testing::Test {
    std::unique_ptr<std::jthread> server_thread_;
    std::shared_ptr<throttr::app> app_;
    int threads_ = 1;

protected:
    void SetUp() override {
        LOG("ServerTestFixture::SetUp scope_in");
        app_ = std::make_shared<throttr::app>(9000, threads_);

        server_thread_ = std::make_unique<std::jthread>([this]() {
            LOG("ServerTestFixture::SetUp thread scope_in");
            app_->serve();
            LOG("ServerTestFixture::SetUp thread scope_out");
        });

        while (!app_->state_->acceptor_ready_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        LOG("ServerTestFixture::SetUp scope_out");
    }

    void TearDown() override {
        LOG("ServerTestFixture::TearDown scope_in");

        LOG("ServerTestFixture::TearDown stopping");
        app_->stop();
        LOG("ServerTestFixture::TearDown stopped");

        LOG("ServerTestFixture::TearDown joining");
        if (server_thread_ && server_thread_->joinable()) {
            server_thread_->join();
            LOG("ServerTestFixture::TearDown joined");
        } else {
            LOG("ServerTestFixture::TearDown not_joined");
        }

        LOG("ServerTestFixture::TearDown scope_out");
    }

    [[nodiscard]] static std::vector<std::byte> send_and_receive(const std::vector<std::byte>& message, bool receives = true) {
        LOG("ServerTestFixture::send_and_receive scope_in");
        boost::asio::io_context _io_context;
        tcp::resolver _resolver(_io_context);

        LOG("ServerTestFixture::send_and_receive resolving");
        const auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(9000));
        LOG("ServerTestFixture::send_and_receive resolved");

        tcp::socket _socket(_io_context);
        LOG("ServerTestFixture::send_and_receive connecting");
        boost::asio::connect(_socket, _endpoints);
        LOG("ServerTestFixture::send_and_receive connected");

        boost::asio::write(_socket, boost::asio::buffer(message.data(), message.size()));


        if (receives) {
            std::vector<std::byte> _response(13);
            boost::asio::read(_socket, boost::asio::buffer(_response.data(), _response.size()));
            return _response;
        }

        LOG("ServerTestFixture::send_and_receive scope_out");
        std::vector<std::byte> _response(0);
        return _response;
    }
};

TEST_F(ServerTestFixture, HandlesSingleValidRequest) {
    LOG("ServerTestFixture::HandlesSingleValidRequest scope_in");

    const auto _buffer = build_request_buffer(
        4,
        {127, 0, 0, 1},
        9000,
        5,
        10000,
        "/test"
    );

    auto _response = send_and_receive(_buffer);

    ASSERT_EQ(_response.size(), 13);

    const bool _can = static_cast<bool>(_response[0]);

    int _available_requests = 0;
    std::memcpy(&_available_requests, _response.data() + 1, sizeof(_available_requests));

    int64_t _ttl = 0;
    std::memcpy(&_ttl, _response.data() + 1 + sizeof(_available_requests), sizeof(_ttl));

    ASSERT_TRUE(_can);
    ASSERT_EQ(_available_requests, 4);
    ASSERT_GT(_ttl, 0);

    LOG("ServerTestFixture::HandlesSingleValidRequest scope_out");
}

TEST_F(ServerTestFixture, HandlesMultipleValidRequests) {
    LOG("ServerTestFixture::HandlesMultipleValidRequests scope_in");

    auto _buffer = build_request_buffer(
        4,
        {127, 0, 0, 1},
        9000,
        3,
        5000,
        "/multi"
    );

    auto _response1 = send_and_receive(_buffer);
    ASSERT_EQ(_response1.size(), 13);
    ASSERT_TRUE(static_cast<bool>(_response1[0]));

    int _available1 = 0;
    std::memcpy(&_available1, _response1.data() + 1, sizeof(_available1));
    ASSERT_EQ(_available1, 2);

    auto _response2 = send_and_receive(_buffer);
    ASSERT_EQ(_response2.size(), 13);
    ASSERT_TRUE(static_cast<bool>(_response2[0]));

    int _available2 = 0;
    std::memcpy(&_available2, _response2.data() + 1, sizeof(_available2));
    ASSERT_EQ(_available2, 1);

    auto _response3 = send_and_receive(_buffer);
    ASSERT_EQ(_response3.size(), 13);
    ASSERT_TRUE(static_cast<bool>(_response3[0]));

    int _available3 = 0;
    std::memcpy(&_available3, _response3.data() + 1, sizeof(_available3));
    ASSERT_EQ(_available3, 0);

    auto _response4 = send_and_receive(_buffer);
    ASSERT_EQ(_response4.size(), 13);
    ASSERT_FALSE(static_cast<bool>(_response4[0]));

    int _available4 = 0;
    std::memcpy(&_available4, _response4.data() + 1, sizeof(_available4));
    ASSERT_EQ(_available4, 0);

    LOG("ServerTestFixture::HandlesMultipleValidRequests scope_out");
}


TEST_F(ServerTestFixture, TTLExpiration) {
    LOG("ServerTestFixture::TTLExpiration scope_in");

    const auto _buffer = build_request_buffer(
        4,
        {127, 0, 0, 1},
        9000,
        10,
        1000,
        "/expire"
    );

    auto _response1 = send_and_receive(_buffer);
    ASSERT_TRUE(static_cast<bool>(_response1[0]));

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    auto _response2 = send_and_receive(_buffer);

    ASSERT_TRUE(static_cast<bool>(_response2[0]));

    int _available = 0;
    std::memcpy(&_available, _response2.data() + 1, sizeof(_available));

    int64_t _ttl = 0;
    std::memcpy(&_ttl, _response2.data() + 1 + sizeof(_available), sizeof(_ttl));

    ASSERT_EQ(_available, 9);
    ASSERT_GT(_ttl, 0);

    LOG("ServerTestFixture::TTLExpiration scope_out");
}

TEST_F(ServerTestFixture, SeparateStocksForDifferentURLs) {
    LOG("ServerTestFixture::SeparateStocksForDifferentURLs scope_in");

    const auto _buffer_a = build_request_buffer(
        4,
        {127, 0, 0, 1},
        9000,
        2,
        5000,
        "/a"
    );

    const auto _buffer_b = build_request_buffer(
        4,
        {127, 0, 0, 1},
        9000,
        2,
        5000,
        "/b"
    );

    auto _response_a1 = send_and_receive(_buffer_a);
    ASSERT_TRUE(static_cast<bool>(_response_a1[0]));

    auto _response_b1 = send_and_receive(_buffer_b);
    ASSERT_TRUE(static_cast<bool>(_response_b1[0]));

    int _available_a1 = 0;
    std::memcpy(&_available_a1, _response_a1.data() + 1, sizeof(_available_a1));
    ASSERT_EQ(_available_a1, 1);

    int _available_b1 = 0;
    std::memcpy(&_available_b1, _response_b1.data() + 1, sizeof(_available_b1));
    ASSERT_EQ(_available_b1, 1);

    LOG("ServerTestFixture::SeparateStocksForDifferentURLs scope_out");
}

TEST_F(ServerTestFixture, HandlesCorruptRequestGracefully) {
    LOG("ServerTestFixture::HandlesCorruptRequestGracefully scope_in");

    const std::vector<std::byte> _corrupt_buffer(5);

    const auto _response = send_and_receive(_corrupt_buffer, false);
    ASSERT_TRUE(_response.empty() || _response.size() == 0);

    const auto _buffer = build_request_buffer(
        4,
        {127, 0, 0, 1},
        9000,
        2,
        5000,
        "/b"
    );

    auto _response_a1 = send_and_receive(_buffer);
    ASSERT_TRUE(static_cast<bool>(_response_a1[0]));

    LOG("ServerTestFixture::HandlesCorruptRequestGracefully scope_out");
}