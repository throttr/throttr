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
#include <boost/endian/conversion.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <format>
#include <thread>
#include <throttr/app.hpp>
#include <throttr/state.hpp>

using boost::asio::ip::tcp;
using namespace throttr;

class RealmTestFixture : public testing::Test
{
  /**
   * Master thread
   */
  std::unique_ptr<std::jthread> master_thread_;

  /**
   * Worker thread
   */
  std::unique_ptr<std::jthread> worker_thread_;

  /**
   * Number of threads
   */
  int threads_ = 1;

protected:
  /**
   * Master application instance
   */
  std::shared_ptr<app> master_;

  /**
   * Master application instance
   */
  std::shared_ptr<app> worker_;

  /**
   * Setup
   */
  void SetUp() override
  {
    auto _master_socket = to_string(boost::uuids::random_generator()());

    program_parameters _master_program_options{
      .socket_ = _master_socket,
      .port_ = 0,
      .has_master_ = false,
    };
    master_ = std::make_shared<app>(_master_program_options);

    master_thread_ = std::make_unique<std::jthread>([this]() { master_->serve(); });

    while (!master_->state_->acceptor_ready_)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    program_parameters _worker_program_options{
      .socket_ = to_string(boost::uuids::random_generator()()),
      .port_ = 0,
      .has_master_ = true,
      .master_socket_ = _master_socket,
    };

    worker_ = std::make_shared<app>(_worker_program_options);

    worker_thread_ = std::make_unique<std::jthread>([this]() { worker_->serve(); });

    while (!worker_->state_->acceptor_ready_)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  /**
   * Tears down
   */
  void TearDown() override
  {
    worker_->stop();

    if (worker_thread_ && worker_thread_->joinable())
    {
      worker_thread_->join();
    }

    master_->stop();

    if (master_thread_ && master_thread_->joinable())
    {
      master_thread_->join();
    }
  }

  [[nodiscard]] unix_socket make_client_connection(boost::asio::io_context &io_context) const
  {
    unix_socket _socket(io_context);
    const unix_endpoint _endpoint(worker_->program_options_.socket_);
    _socket.connect(_endpoint);
    return _socket;
  }

  [[nodiscard]] tcp_socket make_tcp_connection(boost::asio::io_context &io_context) const
  {
    tcp_socket _socket(io_context);
    tcp::resolver _resolver(io_context);
    const auto _endpoints = _resolver.resolve("127.0.0.1", std::to_string(worker_->state_->exposed_port_));
    boost::asio::connect(_socket, _endpoints);
    _socket.set_option(tcp::no_delay(true));
    return _socket;
  }

  template<typename Transport> boost::uuids::uuid get_connection_id(Transport &socket)
  {
    const auto _whoami_buffer = request_whoami_builder();

    std::vector _id_response(17, std::byte{0});
    boost::asio::write(socket, boost::asio::buffer(_whoami_buffer.data(), _whoami_buffer.size()));
    boost::asio::read(socket, boost::asio::buffer(_id_response.data(), _id_response.size()));

    boost::uuids::uuid _uuid{};
    std::memcpy(_uuid.data, _id_response.data() + 1, 16);

    return _uuid;
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
    auto _socket = make_client_connection(_io_context);
    boost::asio::write(_socket, boost::asio::buffer(message.data(), message.size()));

    std::vector _response(length, std::byte{0});
    boost::asio::read(_socket, boost::asio::buffer(_response.data(), _response.size()));
    return _response;
  }
};

#endif // THROTTR_TEST_HPP
