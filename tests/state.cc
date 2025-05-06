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

class StateTestFixture : public testing::Test {
public:
    boost::asio::io_context ioc_;
    std::shared_ptr<state> state_;

    void SetUp() override {
        state_ = std::make_shared<state>(ioc_);
    }
};

auto to_bytes = [](const char* str) {
    return std::vector<std::byte>{
        reinterpret_cast<const std::byte*>(str),
        reinterpret_cast<const std::byte*>(str + std::strlen(str)) // NOSONAR
    };
};

template <typename T>
void test_ttl_change(
    state& _state,
    request_entry& _entry,
    const std::string& _key,
    const ttl_types _ttl_type,
    const change_types _change_type,
    const T _expected)
{
    using namespace throttr;
    using namespace std::chrono;

    request_update_header _header{};
    _header.request_type_ = request_types::update;
    _header.attribute_ = attribute_types::ttl;
    _header.change_ = _change_type;
    _header.value_ = static_cast<value_type>(_expected.count());

    request_update _request{ &_header, _key };

    _entry.ttl_type_ = _ttl_type;
    const auto _now = steady_clock::now();
    const auto _before = _entry.expires_at_;

    const auto _key_bytes = std::vector<std::byte>{
        reinterpret_cast<const std::byte*>(_key.data()),
        reinterpret_cast<const std::byte*>(_key.data() + _key.size())
    };
    const std::span _span_key{_key_bytes};

    ASSERT_TRUE(_state.apply_ttl_change(_entry, _request, _now, _span_key));

    switch (_change_type) {
        case change_types::patch:
            EXPECT_GE(_entry.expires_at_, _now + _expected);
            break;
        case change_types::increase:
            EXPECT_GE(_entry.expires_at_, _before + _expected);
            break;
        case change_types::decrease:
            EXPECT_LE(_entry.expires_at_, _before - _expected);
            break;
    }
}

TEST(State, TTLChange) {
    using namespace throttr;
    using namespace std::chrono;

    boost::asio::io_context _ioc;
    state _state(_ioc);
    request_entry _entry;
    _entry.expires_at_ = steady_clock::now();
    const std::string _key = "user";

    {
        std::vector cases{
            std::make_tuple(ttl_types::nanoseconds, change_types::patch, nanoseconds(32)),
            std::make_tuple(ttl_types::nanoseconds, change_types::increase, nanoseconds(64)),
            std::make_tuple(ttl_types::nanoseconds, change_types::decrease, nanoseconds(16)),
        };
        for (const auto& [t, c, e] : cases)
            test_ttl_change<nanoseconds>(_state, _entry, _key, t, c, e);
    }

    {
        std::vector cases{
            std::make_tuple(ttl_types::milliseconds, change_types::patch, milliseconds(128)),
            std::make_tuple(ttl_types::milliseconds, change_types::increase, milliseconds(16)),
            std::make_tuple(ttl_types::milliseconds, change_types::decrease, milliseconds(32)),
        };
        for (const auto& [t, c, e] : cases)
            test_ttl_change<milliseconds>(_state, _entry, _key, t, c, e);
    }

    {
        std::vector cases{
            std::make_tuple(ttl_types::seconds, change_types::patch, seconds(4)),
            std::make_tuple(ttl_types::seconds, change_types::increase, seconds(1)),
            std::make_tuple(ttl_types::seconds, change_types::decrease, seconds(1)),
        };
        for (const auto& [t, c, e] : cases)
            test_ttl_change<seconds>(_state, _entry, _key, t, c, e);
    }
}

TEST(StateHelpersTest, CalculateExpirationPointNanoseconds) {
    const auto _now = std::chrono::steady_clock::now();
    const auto _expires = get_expiration_point(_now, ttl_types::nanoseconds, 32);

    const auto _diff = std::chrono::duration_cast<std::chrono::nanoseconds>(_expires - _now).count();
    ASSERT_NEAR(_diff, 32, 16);
}

TEST(StateHelpersTest, CalculateExpirationPointSeconds) {
    const auto _now = std::chrono::steady_clock::now();
    const auto _expires = get_expiration_point(_now, ttl_types::seconds, 3);

    const auto _diff = std::chrono::duration_cast<std::chrono::seconds>(_expires - _now).count();
    ASSERT_NEAR(_diff, 3, 1);
}

TEST(StateHelpersTest, CalculateTTLRemainingNanosecondsNotExpired) {
    const auto _now = std::chrono::steady_clock::now();
    const auto _expires = _now + std::chrono::nanoseconds(256);

    const auto _ttl = get_ttl(_expires, ttl_types::nanoseconds);
    ASSERT_GE(_ttl, 0);
}

TEST(StateHelpersTest, CalculateTTLRemainingSecondsNotExpired) {
    const auto _now = std::chrono::steady_clock::now();
    const auto _expires = _now + std::chrono::seconds(10);

    const auto _ttl = get_ttl(_expires, ttl_types::seconds);
    ASSERT_GE(_ttl, 0);
}

TEST(StateHelpersTest, CalculateTTLRemainingNanosecondsExpired) {
    const auto _now = std::chrono::steady_clock::now();
    const auto _expires = _now - std::chrono::nanoseconds(100);

    std::this_thread::sleep_for(std::chrono::nanoseconds(1000));

    const auto _quota = get_ttl(_expires, ttl_types::nanoseconds);
    ASSERT_EQ(_quota, 0);
}

TEST(StateHelpersTest, CalculateTTLRemainingSecondsExpired) {
    const auto _now = std::chrono::steady_clock::now();
    const auto _expires = _now - std::chrono::seconds(1);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    const auto _quota = get_ttl(_expires, ttl_types::seconds);
    ASSERT_EQ(_quota, 0);
}

TEST(State, QuotaChange) {
    request_entry _entry;

    // patch
    _entry.quota_ = 0;
    request_update_header _patch_header{request_types::update, attribute_types::quota, change_types::patch, 42, 0};
    request_update _patch_req{&_patch_header, ""};
    EXPECT_TRUE(state::apply_quota_change(_entry, _patch_req));
    EXPECT_EQ(_entry.quota_, 42);

    // increase
    _entry.quota_ = 10;
    request_update_header _inc_header{request_types::update, attribute_types::quota, change_types::increase, 5, 0};
    request_update _inc_req{&_inc_header, ""};
    EXPECT_TRUE(state::apply_quota_change(_entry, _inc_req));
    EXPECT_EQ(_entry.quota_, 15);

    // decrease (quota > value)
    _entry.quota_ = 20;
    request_update_header _dec_gt_header{request_types::update, attribute_types::quota, change_types::decrease, 10, 0};
    request_update _dec_gt_req{&_dec_gt_header, ""};
    EXPECT_TRUE(state::apply_quota_change(_entry, _dec_gt_req));
    EXPECT_EQ(_entry.quota_, 10);

    // decrease (quota == value)
    _entry.quota_ = 10;
    request_update_header _dec_eq_header{request_types::update, attribute_types::quota, change_types::decrease, 10, 0};
    request_update _dec_eq_req{&_dec_eq_header, ""};
    EXPECT_TRUE(state::apply_quota_change(_entry, _dec_eq_req));
    EXPECT_EQ(_entry.quota_, 0);

    // decrease (quota < value)
    _entry.quota_ = 5;
    request_update_header _dec_lt_header{request_types::update, attribute_types::quota, change_types::decrease, 10, 0};
    request_update _dec_lt_req{&_dec_lt_header, ""};
    EXPECT_FALSE(state::apply_quota_change(_entry, _dec_lt_req));
}

TEST_F(StateTestFixture, ScheduleExpiration_ReprogramsIfNextEntryExists) {
    using namespace std::chrono;

    auto &_storage = state_->storage_;
    auto &_index = _storage.get<tag_by_key>();

    const auto _now = steady_clock::now();

    request_entry _entry1;
    _entry1.quota_ = 1;
    _entry1.expires_at_ = _now;

    request_entry _entry2;
    _entry2.quota_ = 1;
    _entry2.expires_at_ = _now + seconds(5);

    _index.insert(entry_wrapper{to_bytes("c1r1"), _entry1});
    _index.insert(entry_wrapper{to_bytes("c2r2"), _entry2});

    state_->schedule_expiration(_now);

    ioc_.restart();
    ioc_.run_for(seconds(6));

    EXPECT_TRUE(_index.empty());
}


TEST_F(StateTestFixture, CollectAndFlush) {
    using namespace std::chrono;

    auto &_expired = state_->expired_entries_;

    const auto _now = steady_clock::now();

    _expired.emplace_back(entry_wrapper{to_bytes("ab"), {0, ttl_types::seconds, _now - seconds(10)}}, _now - seconds(10));
    _expired.emplace_back(entry_wrapper{to_bytes("ab"), {0, ttl_types::seconds, _now - seconds(6)}}, _now - seconds(6));
    _expired.emplace_back(entry_wrapper{to_bytes("ab"), {0, ttl_types::seconds, _now - seconds(1)}}, _now - seconds(1));

    state_->collect_and_flush();

    ioc_.restart();
    ioc_.run_for(seconds(90));

    ASSERT_EQ(_expired.size(), 0);
}