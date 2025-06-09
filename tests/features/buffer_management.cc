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
#include <throttr/app.hpp>
#include <throttr/state.hpp>

using boost::asio::ip::tcp;
using namespace throttr;

class BufferManagementTestFixture : public ::testing::Test
{
public:
  static connection<tcp_socket> create_dummy_session(const std::shared_ptr<state> &state)
  {
    boost::asio::io_context ioc;
    tcp_socket socket(ioc);
    return connection(std::move(socket), state, connection_type::client);
  }
};

TEST_F(BufferManagementTestFixture, CompactBufferClearsWhenFullyConsumed)
{
  boost::asio::io_context ioc;
  const auto _state = std::make_shared<state>(ioc);
  auto s = create_dummy_session(_state);

  s.buffer_start_ = 100;
  s.buffer_end_ = 100;

  s.compact_buffer_if_needed();

  ASSERT_EQ(s.buffer_start_, 0);
  ASSERT_EQ(s.buffer_end_, 0);
}

TEST_F(BufferManagementTestFixture, CompactBufferCompactsWhenHalfFull)
{
  boost::asio::io_context ioc;
  const auto _state = std::make_shared<state>(ioc);
  auto s = create_dummy_session(_state);

  constexpr std::string_view data = "abcdef";
  std::memcpy(s.buffer_.data() + 6000, data.data(), data.size());
  s.buffer_start_ = 6000;
  s.buffer_end_ = 6000 + data.size();

  s.compact_buffer_if_needed();

  ASSERT_EQ(s.buffer_start_, 0);
  ASSERT_EQ(s.buffer_end_, data.size());

  std::string recovered;
  recovered.reserve(data.size());
  for (std::size_t i = 0; i < data.size(); ++i)
  {
    recovered += static_cast<char>(std::to_integer<uint8_t>(s.buffer_[i]));
  }
  ASSERT_EQ(recovered, data);
}