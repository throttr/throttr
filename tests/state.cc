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

using boost::asio::ip::tcp;
using namespace throttr;

TEST(StateHelpersTest, CalculateExpirationPointNanoseconds) {
    const auto _now = std::chrono::steady_clock::now();
    const auto _expires = get_expiration_point(_now, ttl_types::nanoseconds, 1000);

    const auto _diff = std::chrono::duration_cast<std::chrono::nanoseconds>(_expires - _now).count();
    ASSERT_NEAR(_diff, 1000, 500);
}

TEST(StateHelpersTest, CalculateExpirationPointSeconds) {
    const auto _now = std::chrono::steady_clock::now();
    const auto _expires = get_expiration_point(_now, ttl_types::seconds, 3);

    const auto _diff = std::chrono::duration_cast<std::chrono::seconds>(_expires - _now).count();
    ASSERT_NEAR(_diff, 3, 1);
}

TEST(StateHelpersTest, CalculateTTLRemainingNanosecondsNotExpired) {
    const auto _now = std::chrono::steady_clock::now();
    const auto _expires = _now + std::chrono::nanoseconds(10'000'000);

    const auto _remaining = get_ttl(_expires, ttl_types::nanoseconds);
    ASSERT_GT(_remaining, 0);
}

TEST(StateHelpersTest, CalculateTTLRemainingSecondsNotExpired) {
    const auto _now = std::chrono::steady_clock::now();
    const auto _expires = _now + std::chrono::seconds(10);

    const auto _remaining = get_ttl(_expires, ttl_types::seconds);
    ASSERT_GE(_remaining, 0);
}

TEST(StateHelpersTest, CalculateTTLRemainingNanosecondsExpired) {
    const auto _now = std::chrono::steady_clock::now();
    const auto _expires = _now - std::chrono::nanoseconds(100);

    std::this_thread::sleep_for(std::chrono::nanoseconds(1000));

    const auto _remaining = get_ttl(_expires, ttl_types::nanoseconds);
    ASSERT_EQ(_remaining, 0);
}

TEST(StateHelpersTest, CalculateTTLRemainingSecondsExpired) {
    const auto _now = std::chrono::steady_clock::now();
    const auto _expires = _now - std::chrono::seconds(1);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    const auto _remaining = get_ttl(_expires, ttl_types::seconds);
    ASSERT_EQ(_remaining, 0);
}