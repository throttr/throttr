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
    const std::string& url
) {
    std::vector<std::byte> buffer;
    buffer.resize(sizeof(throttr::request_header) + url.size());

    auto* header = reinterpret_cast<throttr::request_header*>(buffer.data());

    header->ip_version_ = ip_version;
    header->ip_ = ip;
    header->port_ = port;
    header->max_requests_ = max_requests;
    header->ttl_ = ttl;
    header->size_ = static_cast<uint8_t>(url.size());

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

    [[nodiscard]] static std::vector<std::byte> send_and_receive(const std::vector<std::byte>& message) {
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

        std::vector<std::byte> _response(13);
        boost::asio::read(_socket, boost::asio::buffer(_response.data(), _response.size()));

        LOG("ServerTestFixture::send_and_receive scope_out");
        return _response;
    }
};

TEST_F(ServerTestFixture, HandlesSingleValidRequest) {
    LOG("ServerTestFixture::HandlesSingleValidRequest scope_in");

    const auto buffer = build_request_buffer(
        4,
        {127, 0, 0, 1},
        9000,
        5,
        10000,
        "/test"
    );

    auto response = send_and_receive(buffer);

    ASSERT_EQ(response.size(), 13);

    const bool can = static_cast<bool>(response[0]);

    int available_requests = 0;
    std::memcpy(&available_requests, response.data() + 1, sizeof(available_requests));

    int64_t ttl_ms = 0;
    std::memcpy(&ttl_ms, response.data() + 1 + sizeof(available_requests), sizeof(ttl_ms));

    ASSERT_TRUE(can);
    ASSERT_EQ(available_requests, 4);
    ASSERT_GT(ttl_ms, 0);

    LOG("ServerTestFixture::HandlesSingleValidRequest scope_out");
}

TEST_F(ServerTestFixture, HandlesMultipleValidRequests) {
    LOG("ServerTestFixture::HandlesMultipleValidRequests scope_in");

    auto buffer = build_request_buffer(
        4,
        {127, 0, 0, 1},
        9000,
        3,
        5000,
        "/multi"
    );

    auto response1 = send_and_receive(buffer);
    ASSERT_EQ(response1.size(), 13);
    ASSERT_TRUE(static_cast<bool>(response1[0]));

    int available1 = 0;
    std::memcpy(&available1, response1.data() + 1, sizeof(available1));
    ASSERT_EQ(available1, 2);

    auto response2 = send_and_receive(buffer);
    ASSERT_EQ(response2.size(), 13);
    ASSERT_TRUE(static_cast<bool>(response2[0]));

    int available2 = 0;
    std::memcpy(&available2, response2.data() + 1, sizeof(available2));
    ASSERT_EQ(available2, 1);

    auto response3 = send_and_receive(buffer);
    ASSERT_EQ(response3.size(), 13);
    ASSERT_TRUE(static_cast<bool>(response3[0]));

    int available3 = 0;
    std::memcpy(&available3, response3.data() + 1, sizeof(available3));
    ASSERT_EQ(available3, 0);

    auto response4 = send_and_receive(buffer);
    ASSERT_EQ(response4.size(), 13);
    ASSERT_FALSE(static_cast<bool>(response4[0]));

    int available4 = 0;
    std::memcpy(&available4, response4.data() + 1, sizeof(available4));
    ASSERT_EQ(available4, 0);

    LOG("ServerTestFixture::HandlesMultipleValidRequests scope_out");
}


TEST_F(ServerTestFixture, TTLExpiration) {
    LOG("ServerTestFixture::TTLExpiration scope_in");

    const auto buffer = build_request_buffer(
        4,
        {127, 0, 0, 1},
        9000,
        10,
        1000,
        "/expire"
    );

    auto response1 = send_and_receive(buffer);
    ASSERT_TRUE(static_cast<bool>(response1[0]));

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    auto response2 = send_and_receive(buffer);

    ASSERT_TRUE(static_cast<bool>(response2[0]));

    int available = 0;
    std::memcpy(&available, response2.data() + 1, sizeof(available));

    int64_t ttl_ms = 0;
    std::memcpy(&ttl_ms, response2.data() + 1 + sizeof(available), sizeof(ttl_ms));

    ASSERT_EQ(available, 9);
    ASSERT_GT(ttl_ms, 0);

    LOG("ServerTestFixture::TTLExpiration scope_out");
}

TEST_F(ServerTestFixture, SeparateStocksForDifferentURLs) {
    LOG("ServerTestFixture::SeparateStocksForDifferentURLs scope_in");

    const auto buffer_a = build_request_buffer(
        4,
        {127, 0, 0, 1},
        9000,
        2,
        5000,
        "/a"
    );

    const auto buffer_b = build_request_buffer(
        4,
        {127, 0, 0, 1},
        9000,
        2,
        5000,
        "/b"
    );

    auto response_a1 = send_and_receive(buffer_a);
    ASSERT_TRUE(static_cast<bool>(response_a1[0]));

    auto response_b1 = send_and_receive(buffer_b);
    ASSERT_TRUE(static_cast<bool>(response_b1[0]));

    int available_a1 = 0;
    std::memcpy(&available_a1, response_a1.data() + 1, sizeof(available_a1));
    ASSERT_EQ(available_a1, 1);

    int available_b1 = 0;
    std::memcpy(&available_b1, response_b1.data() + 1, sizeof(available_b1));
    ASSERT_EQ(available_b1, 1);

    LOG("ServerTestFixture::SeparateStocksForDifferentURLs scope_out");
}