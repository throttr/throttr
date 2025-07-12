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

#include <throttr/utils.hpp>

#include <gtest/gtest.h>

#include <throttr/protocol_wrapper.hpp>
#include <throttr/version.hpp>

TEST(Throttr, Version)
{
  ASSERT_EQ(throttr::get_version(), "5.0.10");
}

TEST(Throttr, Translations)
{
  ASSERT_EQ(throttr::to_string(throttr::ttl_types::nanoseconds), "nanoseconds");
  ASSERT_EQ(throttr::to_string(throttr::ttl_types::microseconds), "microseconds");
  ASSERT_EQ(throttr::to_string(throttr::ttl_types::milliseconds), "milliseconds");
  ASSERT_EQ(throttr::to_string(throttr::ttl_types::minutes), "minutes");
  ASSERT_EQ(throttr::to_string(throttr::ttl_types::hours), "hours");

  ASSERT_EQ(throttr::to_string(throttr::attribute_types::quota), "quota");
  ASSERT_EQ(throttr::to_string(throttr::attribute_types::ttl), "ttl");

  ASSERT_EQ(throttr::to_string(throttr::change_types::increase), "increase");
  ASSERT_EQ(throttr::to_string(throttr::change_types::decrease), "decrease");
  ASSERT_EQ(throttr::to_string(throttr::change_types::patch), "patch");
}