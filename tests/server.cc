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

class EchoServerTestFixture : public testing::Test {
    std::unique_ptr<std::jthread> server_thread_;
    std::shared_ptr<throttr::app> app_;
    int threads_ = 1;

protected:
    void SetUp() override {
        LOG("EchoServerTestFixture::SetUp scope_in");
        app_ = std::make_shared<throttr::app>(9000, threads_);

        server_thread_ = std::make_unique<std::jthread>([this]() {
            LOG("EchoServerTestFixture::SetUp thread scope_in");
            app_->serve();
            LOG("EchoServerTestFixture::SetUp thread scope_out");
        });

        while (!app_->state_->acceptor_ready_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        LOG("EchoServerTestFixture::SetUp scope_out");
    }

    void TearDown() override {
        LOG("EchoServerTestFixture::TearDown scope_in");

        LOG("EchoServerTestFixture::TearDown stopping");
        app_->stop();
        LOG("EchoServerTestFixture::TearDown stopped");

        LOG("EchoServerTestFixture::TearDown joining");
        if (server_thread_ && server_thread_->joinable()) {
            server_thread_->join();
            LOG("EchoServerTestFixture::TearDown joined");
        } else {
            LOG("EchoServerTestFixture::TearDown not_joined");
        }

        LOG("EchoServerTestFixture::TearDown scope_out");
    }

    [[nodiscard]] static std::string send_and_receive(const std::string& message) {
        LOG("EchoServerTestFixture::send_and_receive scope_in");
        boost::asio::io_context _io_context;
        tcp::resolver _resolver(_io_context);

        LOG("EchoServerTestFixture::send_and_receive resolving");
        auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(9000));
        LOG("EchoServerTestFixture::send_and_receive resolved");

        tcp::socket _socket(_io_context);
        LOG("EchoServerTestFixture::send_and_receive connecting");
        boost::asio::connect(_socket, _endpoints);
        LOG("EchoServerTestFixture::send_and_receive connected");

        boost::asio::write(_socket, boost::asio::buffer(message));

        std::string _response(1024, '\0');
        const size_t _response_length = _socket.read_some(boost::asio::buffer(_response));
        _response.resize(_response_length);

        LOG("EchoServerTestFixture::send_and_receive scope_out");
        return _response;
    }
};

TEST_F(EchoServerTestFixture, EchoesSingleMessage) {
    LOG("EchoServerTestFixture::EchoesSingleMessage scope_in");
    const std::string _message = "Hola Throttr!";
    const std::string _response = send_and_receive(_message);
    ASSERT_EQ(_response, _message);
    LOG("EchoServerTestFixture::EchoesSingleMessage scope_out");
}

TEST_F(EchoServerTestFixture, EchoesMultipleMessages) {
    LOG("EchoServerTestFixture::EchoesMultipleMessages scope_in");
    std::string _first_message = "Ping";
    std::string _second_message = "Pong";

    boost::asio::io_context _io_context;
    tcp::resolver _resolver(_io_context);

    LOG("EchoServerTestFixture::EchoesMultipleMessages resolving");
    const auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(9000));
    LOG("EchoServerTestFixture::EchoesMultipleMessages resolved");

    tcp::socket _socket(_io_context);

    LOG("EchoServerTestFixture::EchoesMultipleMessages connecting");
    boost::asio::connect(_socket, _endpoints);
    LOG("EchoServerTestFixture::EchoesMultipleMessages connected");

    LOG("EchoServerTestFixture::EchoesMultipleMessages writing first message");
    boost::asio::write(_socket, boost::asio::buffer(_first_message));
    LOG("EchoServerTestFixture::EchoesMultipleMessages wrote first message");

    std::string _ping_response_buffer(1024, '\0');
    LOG("EchoServerTestFixture::EchoesMultipleMessages reading first response");
    const size_t _ping_response_length = _socket.read_some(boost::asio::buffer(_ping_response_buffer.data(), _ping_response_buffer.size()));
    _ping_response_buffer.resize(_ping_response_length);
    ASSERT_EQ(_ping_response_buffer, _first_message);
    LOG("EchoServerTestFixture::EchoesMultipleMessages read first response");

    LOG("EchoServerTestFixture::EchoesMultipleMessages writing second message");
    boost::asio::write(_socket, boost::asio::buffer(_second_message));
    LOG("EchoServerTestFixture::EchoesMultipleMessages wrote second message");

    std::string _pong_response_buffer(1024, '\0');
    LOG("EchoServerTestFixture::EchoesMultipleMessages reading second response");
    const size_t _pong_response_length = _socket.read_some(boost::asio::buffer(_pong_response_buffer.data(), _pong_response_buffer.size()));
    _pong_response_buffer.resize(_pong_response_length);
    LOG("EchoServerTestFixture::EchoesMultipleMessages read second response");

    ASSERT_EQ(_pong_response_buffer, _second_message);
    LOG("EchoServerTestFixture::EchoesMultipleMessages scope_out");
}
