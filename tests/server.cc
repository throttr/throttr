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

    [[nodiscard]] std::string send_and_receive(const std::string& message) const {
        LOG("EchoServerTestFixture::send_and_receive scope_in");
        boost::asio::io_context context;
        tcp::resolver resolver(context);

        LOG("EchoServerTestFixture::send_and_receive resolving");
        auto endpoints = resolver.resolve("127.0.0.1", std::to_string(9000));
        LOG("EchoServerTestFixture::send_and_receive resolved");

        tcp::socket socket(context);
        LOG("EchoServerTestFixture::send_and_receive connecting");
        boost::asio::connect(socket, endpoints);
        LOG("EchoServerTestFixture::send_and_receive connected");

        boost::asio::write(socket, boost::asio::buffer(message));

        std::string reply(1024, '\0');
        const size_t reply_length = socket.read_some(boost::asio::buffer(reply));
        reply.resize(reply_length);

        LOG("EchoServerTestFixture::send_and_receive scope_out");
        return reply;
    }
};

TEST_F(EchoServerTestFixture, EchoesSingleMessage) {
        LOG("EchoServerTestFixture::EchoesSingleMessage scope_in");
    const std::string msg = "Hola Throttr!";
    const std::string response = send_and_receive(msg);
    ASSERT_EQ(response, msg);
        LOG("EchoServerTestFixture::EchoesSingleMessage scope_out");
}

TEST_F(EchoServerTestFixture, EchoesMultipleMessages) {
    LOG("EchoServerTestFixture::EchoesMultipleMessages scope_in");
    std::string msg = "Hola Throttr!";
    std::string first = "Ping";
    std::string second = "Pong";

    boost::asio::io_context context;
    tcp::resolver resolver(context);

    LOG("EchoServerTestFixture::EchoesMultipleMessages resolving");
    const auto endpoints = resolver.resolve("127.0.0.1", std::to_string(9000));
    LOG("EchoServerTestFixture::EchoesMultipleMessages resolved");

    tcp::socket socket(context);

    LOG("EchoServerTestFixture::EchoesMultipleMessages connecting");
    boost::asio::connect(socket, endpoints);
    LOG("EchoServerTestFixture::EchoesMultipleMessages connected");

    LOG("EchoServerTestFixture::EchoesMultipleMessages writing 1 of 2");
    boost::asio::write(socket, boost::asio::buffer(first));
    LOG("EchoServerTestFixture::EchoesMultipleMessages wrote 1 of 2");
    std::string buffer1(1024, '\0');

    LOG("EchoServerTestFixture::EchoesMultipleMessages reading 1 of 2");
    const size_t len1 = socket.read_some(boost::asio::buffer(buffer1.data(), buffer1.size()));
    buffer1.resize(len1);
    ASSERT_EQ(buffer1, first);
    LOG("EchoServerTestFixture::EchoesMultipleMessages read 2 of 2");

    LOG("EchoServerTestFixture::EchoesMultipleMessages writing 2 of 2");
    boost::asio::write(socket, boost::asio::buffer(second));
    LOG("EchoServerTestFixture::EchoesMultipleMessages wrote 2 of 2");
    std::string buffer2(1024, '\0');

    LOG("EchoServerTestFixture::EchoesMultipleMessages reading 1 of 2");
    const size_t len2 = socket.read_some(boost::asio::buffer(buffer2.data(), buffer2.size()));
    buffer2.resize(len2);
    LOG("EchoServerTestFixture::EchoesMultipleMessages read 2 of 2");

    ASSERT_EQ(buffer2, second);
    LOG("EchoServerTestFixture::EchoesMultipleMessages scope_out");
}
