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

#include "../service_test_fixture.hpp"

#ifdef ENABLED_FEATURE_METRICS
class InfoTestFixture : public ServiceTestFixture
{
};

TEST_F(InfoTestFixture, OnSuccessSingleFragment)
{
  const auto _buffer = request_info_builder();
  const auto _response = send_and_receive(_buffer, 425);

  size_t _offset = 0;

  const auto _status = std::to_integer<uint8_t>(_response[_offset++]);
  ASSERT_EQ(_status, 0x01);

  ASSERT_EQ(_response.size(), 425);
}
#endif