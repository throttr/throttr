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

#pragma once

#ifndef THROTTR_TEST_HPP
#define THROTTR_TEST_HPP

#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <thread>
#include <throttr/app.hpp>
#include <throttr/state.hpp>

using boost::asio::ip::tcp;
using namespace throttr;

class ServiceTestFixture : public testing::Test
{
  /**
   * Server thread
   */
  std::unique_ptr<std::jthread> server_thread_;

  /**
   * Number of threads
   */
  int threads_ = 1;

protected:
  /**
   * Application instance
   */
  std::shared_ptr<app> app_;

  /**
   * Setup
   */
  void SetUp() override
  {
    app_ = std::make_shared<app>(0, threads_);

    server_thread_ = std::make_unique<std::jthread>([this]() { app_->serve(); });

    while (!app_->state_->acceptor_ready_)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  /**
   * Tears down
   */
  void TearDown() override
  {
    app_->stop();

    if (server_thread_ && server_thread_->joinable())
    {
      server_thread_->join();
    }
  }

  /**
   * Send and receive
   *
   * @param message
   * @param length
   * @return
   */
  [[nodiscard]] std::vector<std::byte> send_and_receive(const std::vector<std::byte> &message, const int length = 1) const
  {
    boost::asio::io_context _io_context;
    tcp::resolver _resolver(_io_context);

    const auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(app_->state_->exposed_port_));

    tcp::socket _socket(_io_context);
    boost::asio::connect(_socket, _endpoints);

    boost::asio::write(_socket, boost::asio::buffer(message.data(), message.size()));

    std::vector<std::byte> _response(length);
    boost::asio::read(_socket, boost::asio::buffer(_response.data(), _response.size()));

    return _response;
  }
};

#endif // THROTTR_TEST_HPP
