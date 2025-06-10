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
#include <throttr/services/garbage_collector_service.hpp>
#include <throttr/services/update_service.hpp>
#include <throttr/state.hpp>
#include <throttr/time.hpp>

using boost::asio::ip::tcp;
using namespace throttr;

class StateManagementTestFixture : public testing::Test
{
public:
  boost::asio::io_context ioc_;
  std::shared_ptr<state> state_;

  void SetUp() override
  {
    state_ = std::make_shared<state>(ioc_);
  }
};

auto to_bytes = [](const char *str)
{
  return std::vector<std::byte>{
    reinterpret_cast<const std::byte *>(str),
    reinterpret_cast<const std::byte *>(str + std::strlen(str)) // NOSONAR
  };
};

template<typename T>
void
test_ttl_change(
  std::shared_ptr<state> &_state,
  entry &_entry,
  const std::vector<std::byte> &_key,
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

  const request_update _request{_header.attribute_, _header.change_, _header.value_, _key};

  _entry.ttl_type_ = _ttl_type;
  const auto _now = steady_clock::now().time_since_epoch().count();
  const auto _before = _entry.expires_at_.load(std::memory_order_relaxed);

  const auto _key_bytes = std::vector<
    std::byte>{reinterpret_cast<const std::byte *>(_key.data()), reinterpret_cast<const std::byte *>(_key.data() + _key.size())};
  const std::span _span_key{_key_bytes};

  ASSERT_TRUE(update_service::apply_ttl_change(_state, _entry, _request, _now, _span_key));

  switch (_change_type)
  {
    case change_types::patch:
      EXPECT_GE(
        _entry.expires_at_.load(std::memory_order_relaxed),
        _now + std::chrono::duration_cast<std::chrono::nanoseconds>(_expected).count());
      break;
    case change_types::increase:
      EXPECT_GE(
        _entry.expires_at_.load(std::memory_order_relaxed),
        _before + std::chrono::duration_cast<std::chrono::nanoseconds>(_expected).count());
      break;
    case change_types::decrease:
      EXPECT_LE(
        _entry.expires_at_.load(std::memory_order_relaxed),
        _before - std::chrono::duration_cast<std::chrono::nanoseconds>(_expected).count());
      break;
  }
}

TEST(StateManagementTest, TTLChange)
{
  using namespace throttr;
  using namespace std::chrono;

  boost::asio::io_context _ioc;
  auto _state = std::make_shared<state>(_ioc);
  entry _entry;
  _entry.expires_at_.store(steady_clock::now().time_since_epoch().count(), std::memory_order_relaxed);
  const std::vector _key = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04}};

  {
    std::vector cases{
      std::make_tuple(ttl_types::nanoseconds, change_types::patch, nanoseconds(32)),
      std::make_tuple(ttl_types::nanoseconds, change_types::increase, nanoseconds(64)),
      std::make_tuple(ttl_types::nanoseconds, change_types::decrease, nanoseconds(16)),
    };
    for (const auto &[t, c, e] : cases)
      test_ttl_change<nanoseconds>(_state, _entry, _key, t, c, e);
  }

  {
    std::vector cases{
      std::make_tuple(ttl_types::milliseconds, change_types::patch, milliseconds(128)),
      std::make_tuple(ttl_types::milliseconds, change_types::increase, milliseconds(16)),
      std::make_tuple(ttl_types::milliseconds, change_types::decrease, milliseconds(32)),
    };
    for (const auto &[t, c, e] : cases)
      test_ttl_change<milliseconds>(_state, _entry, _key, t, c, e);
  }

  {
    std::vector cases{
      std::make_tuple(ttl_types::seconds, change_types::patch, seconds(4)),
      std::make_tuple(ttl_types::seconds, change_types::increase, seconds(1)),
      std::make_tuple(ttl_types::seconds, change_types::decrease, seconds(1)),
    };
    for (const auto &[t, c, e] : cases)
      test_ttl_change<seconds>(_state, _entry, _key, t, c, e);
  }

  {
    std::vector cases{
      std::make_tuple(ttl_types::microseconds, change_types::patch, microseconds(32)),
      std::make_tuple(ttl_types::microseconds, change_types::increase, microseconds(64)),
      std::make_tuple(ttl_types::microseconds, change_types::decrease, microseconds(16)),
    };
    for (const auto &[t, c, e] : cases)
      test_ttl_change<microseconds>(_state, _entry, _key, t, c, e);
  }

  {
    std::vector cases{
      std::make_tuple(ttl_types::hours, change_types::patch, hours(32)),
      std::make_tuple(ttl_types::hours, change_types::increase, hours(64)),
      std::make_tuple(ttl_types::hours, change_types::decrease, hours(16)),
    };
    for (const auto &[t, c, e] : cases)
      test_ttl_change<hours>(_state, _entry, _key, t, c, e);
  }

  {
    std::vector cases{
      std::make_tuple(ttl_types::minutes, change_types::patch, minutes(32)),
      std::make_tuple(ttl_types::minutes, change_types::increase, minutes(64)),
      std::make_tuple(ttl_types::minutes, change_types::decrease, minutes(16)),
    };
    for (const auto &[t, c, e] : cases)
      test_ttl_change<minutes>(_state, _entry, _key, t, c, e);
  }
}

TEST(StateManagementTest, CalculateExpirationPointNanoseconds)
{
  const auto _now_tp = std::chrono::steady_clock::now();
  const auto _now_ns = _now_tp.time_since_epoch().count();

  constexpr value_type _expiration_value = 32;
  std::vector<std::byte> _expiration_bytes(sizeof(value_type));
  std::memcpy(_expiration_bytes.data(), &_expiration_value, sizeof(value_type));
  const auto _expires_ns = get_expiration_point(_now_ns, ttl_types::nanoseconds, _expiration_bytes);

  const auto _diff = static_cast<int64_t>(_expires_ns - _now_ns);
  ASSERT_NEAR(_diff, 32, 16);
}

TEST(StateManagementTest, CalculateExpirationPointSeconds)
{
  const auto _now_tp = std::chrono::steady_clock::now();
  const auto _now_ns = _now_tp.time_since_epoch().count();

  value_type _expiration_value = 3;
  std::vector<std::byte> _expiration_bytes(sizeof(value_type));
  std::memcpy(_expiration_bytes.data(), &_expiration_value, sizeof(value_type));

  const auto _expires_ns = get_expiration_point(_now_ns, ttl_types::seconds, _expiration_bytes);

  const auto _diff_ns = static_cast<int64_t>(_expires_ns - _now_ns);
  const auto _diff_sec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::nanoseconds(_diff_ns)).count();

  ASSERT_NEAR(_diff_sec, 3, 1);
}

TEST(StateManagementTest, CalculateTTLRemainingNanosecondsNotExpired)
{
  const auto _now_tp = std::chrono::steady_clock::now();
  const auto _now_ns = _now_tp.time_since_epoch().count();

  const auto _expires_ns = _now_ns + 256;

  const auto _ttl = get_ttl(_expires_ns, ttl_types::nanoseconds);
  ASSERT_GE(_ttl, 0);
}

TEST(StateManagementTest, CalculateTTLRemainingSecondsNotExpired)
{
  const auto _now_tp = std::chrono::steady_clock::now();
  const auto _now_ns = _now_tp.time_since_epoch().count();

  const auto _expires_ns = _now_ns + std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(10)).count();

  const auto _ttl = get_ttl(_expires_ns, ttl_types::seconds);
  ASSERT_GE(_ttl, 0);
}

TEST(StateManagementTest, CalculateTTLRemainingNanosecondsExpired)
{
  const auto _now_tp = std::chrono::steady_clock::now();
  const auto _now_ns = _now_tp.time_since_epoch().count();

  const auto _expires_ns = _now_ns - 100;

  std::this_thread::sleep_for(std::chrono::nanoseconds(1000));

  const auto _quota = get_ttl(_expires_ns, ttl_types::nanoseconds);
  ASSERT_EQ(_quota, 0);
}

TEST(StateManagementTest, CalculateTTLRemainingSecondsExpired)
{
  const auto _now_tp = std::chrono::steady_clock::now();
  const auto _now_ns = _now_tp.time_since_epoch().count();

  const auto _expires_ns = _now_ns - std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count();

  std::this_thread::sleep_for(std::chrono::seconds(2));

  const auto _quota = get_ttl(_expires_ns, ttl_types::seconds);
  ASSERT_EQ(_quota, 0);
}

TEST(StateManagementTest, QuotaChange)
{
  boost::asio::io_context _ioc;
  auto _state = std::make_shared<state>(_ioc);
  entry _entry;
  _entry.buffer_.store(std::make_shared<std::vector<std::byte>>(sizeof(value_type)), std::memory_order_release);

  // patch
  _entry.counter_.store(0, std::memory_order_relaxed);
  request_update _patch_req{attribute_types::quota, change_types::patch, 42, std::vector{std::byte{0x00}}};
  EXPECT_TRUE(update_service::apply_quota_change(_state, _entry, _patch_req));
  EXPECT_EQ(_entry.counter_.load(std::memory_order_relaxed), 42);

  // increase
  _entry.counter_.store(10, std::memory_order_relaxed);
  request_update _inc_req{attribute_types::quota, change_types::increase, 5, std::vector{std::byte{0x00}}};
  EXPECT_TRUE(update_service::apply_quota_change(_state, _entry, _inc_req));
  EXPECT_EQ(_entry.counter_.load(std::memory_order_relaxed), 15);

  // decrease (quota > value)
  _entry.counter_.store(20, std::memory_order_relaxed);
  request_update _dec_gt_req{attribute_types::quota, change_types::decrease, 10, std::vector{std::byte{0x00}}};
  EXPECT_TRUE(update_service::apply_quota_change(_state, _entry, _dec_gt_req));
  EXPECT_EQ(_entry.counter_.load(std::memory_order_relaxed), 10);

  // decrease (quota == value)
  _entry.counter_.store(10, std::memory_order_relaxed);
  request_update _dec_eq_req{attribute_types::quota, change_types::decrease, 10, std::vector{std::byte{0x00}}};
  EXPECT_TRUE(update_service::apply_quota_change(_state, _entry, _dec_eq_req));
  EXPECT_EQ(_entry.counter_.load(std::memory_order_relaxed), 0);

  // decrease (quota < value)
  _entry.counter_.store(5, std::memory_order_relaxed);
  request_update _dec_lt_req{attribute_types::quota, change_types::decrease, 10, std::vector{std::byte{0x00}}};
  EXPECT_FALSE(update_service::apply_quota_change(_state, _entry, _dec_lt_req));
}

TEST_F(StateManagementTestFixture, ScheduleExpiration_ReprogramsIfNextEntryExists)
{
  using namespace std::chrono;
  auto &_storage = state_->storage_;
  auto &_index = _storage.get<tag_by_key>();

  const std::uint64_t now_ns = steady_clock::now().time_since_epoch().count();
  const std::uint64_t expires1_ns = now_ns;
  const std::uint64_t expires2_ns = now_ns + duration_cast<nanoseconds>(seconds(5)).count();

  entry _entry1;
  _entry1.type_ = entry_types::counter;
  _entry1.counter_.store(32, std::memory_order_release);
  _entry1.expires_at_.store(expires1_ns, std::memory_order_release);

  entry _entry2;
  _entry2.type_ = entry_types::raw;
  _entry2.buffer_.store(std::make_shared<std::vector<std::byte>>(1, std::byte{1}), std::memory_order_release);
  _entry2.expires_at_.store(expires2_ns, std::memory_order_release);

  _index.insert(entry_wrapper{to_bytes("c1r1"), std::move(_entry1)});
  _index.insert(entry_wrapper{to_bytes("c2r2"), std::move(_entry2)});

  state_->garbage_collector_->schedule_timer(state_, now_ns);

  ioc_.restart();
  ioc_.run_for(seconds(30));

  EXPECT_TRUE(_index.empty());
}

TEST_F(StateManagementTestFixture, StateCanPersistKeys)
{
  using namespace std::chrono;
  auto &_storage = state_->storage_;
  auto &_index = _storage.get<tag_by_key>();

  const std::uint64_t now_ns = steady_clock::now().time_since_epoch().count();
  const std::uint64_t expires1_ns = now_ns + duration_cast<nanoseconds>(minutes(30)).count();
  const std::uint64_t expires2_ns = now_ns + duration_cast<nanoseconds>(minutes(60)).count();

  entry _entry1;
  _entry1.type_ = entry_types::counter;
  _entry1.counter_.store(32, std::memory_order_release);
  _entry1.expires_at_.store(expires1_ns, std::memory_order_release);
  auto _entry1wrapper = entry_wrapper{to_bytes("c1r1"), std::move(_entry1)};
#ifdef ENABLED_FEATURE_METRICS
  _entry1wrapper.metrics_->reads_.store(3, std::memory_order_release);
  _entry1wrapper.metrics_->reads_accumulator_.store(66, std::memory_order_release);
  _entry1wrapper.metrics_->writes_per_minute_.store(30, std::memory_order_release);
  _entry1wrapper.metrics_->writes_.store(5, std::memory_order_release);
  _entry1wrapper.metrics_->writes_accumulator_.store(10, std::memory_order_release);
  _entry1wrapper.metrics_->writes_per_minute_.store(33, std::memory_order_release);
#endif

  entry _entry2;
  _entry2.type_ = entry_types::raw;
  _entry2.buffer_.store(std::make_shared<std::vector<std::byte>>(1, std::byte{1}), std::memory_order_release);
  _entry2.expires_at_.store(expires2_ns, std::memory_order_release);
  auto _entry2wrapper = entry_wrapper{to_bytes("c2r2"), std::move(_entry2)};
#ifdef ENABLED_FEATURE_METRICS
  _entry2wrapper.metrics_->reads_.store(5, std::memory_order_release);
  _entry2wrapper.metrics_->reads_accumulator_.store(11, std::memory_order_release);
  _entry2wrapper.metrics_->writes_per_minute_.store(13, std::memory_order_release);
  _entry2wrapper.metrics_->writes_.store(15, std::memory_order_release);
  _entry2wrapper.metrics_->writes_accumulator_.store(17, std::memory_order_release);
  _entry2wrapper.metrics_->writes_per_minute_.store(19, std::memory_order_release);
#endif

  _index.insert(_entry1wrapper);
  _index.insert(_entry2wrapper);

  EXPECT_EQ(_index.size(), 2);

  const program_parameters _parameters = {
    .persistent_ = true,
    .dump_ = to_string(boost::uuids::random_generator()()),
  };
  state_->prepare_for_shutdown(_parameters);
  state_->storage_.clear();
  EXPECT_TRUE(state_->storage_.empty());

  state_->prepare_for_startup(_parameters);

  EXPECT_FALSE(state_->storage_.empty());
  auto &_recovered_index = _storage.get<tag_by_key>();
  EXPECT_EQ(_recovered_index.size(), 2);

  auto _it1 = _recovered_index.find(request_key{std::string_view("c1r1")});
  ASSERT_NE(_it1, _recovered_index.end());
  const auto &_scoped_entry1 = *_it1;
  EXPECT_EQ(_scoped_entry1.entry_.type_, entry_types::counter);
  EXPECT_EQ(_scoped_entry1.entry_.counter_.load(std::memory_order_relaxed), 32);

  auto _it2 = _recovered_index.find(request_key{std::string_view("c2r2")});
  ASSERT_NE(_it2, _recovered_index.end());
  const auto &_scoped_entry2 = *_it2;
  EXPECT_EQ(_scoped_entry2.entry_.type_, entry_types::raw);
  const auto _raw_ptr = _scoped_entry2.entry_.buffer_.load();
  ASSERT_EQ(_raw_ptr->size(), 1);
  EXPECT_EQ((*_raw_ptr)[0], std::byte{1});

  EXPECT_EQ(_scoped_entry1.entry_.expires_at_.load(std::memory_order_relaxed), expires1_ns);
  EXPECT_EQ(_scoped_entry2.entry_.expires_at_.load(std::memory_order_relaxed), expires2_ns);

#ifdef ENABLED_FEATURE_METRICS
  EXPECT_EQ(_scoped_entry1.metrics_->reads_.load(std::memory_order_relaxed), 3);
  EXPECT_EQ(_scoped_entry1.metrics_->reads_accumulator_.load(std::memory_order_relaxed), 66);
  EXPECT_EQ(_scoped_entry1.metrics_->writes_.load(std::memory_order_relaxed), 5);
  EXPECT_EQ(_scoped_entry1.metrics_->writes_accumulator_.load(std::memory_order_relaxed), 10);
  EXPECT_EQ(_scoped_entry1.metrics_->writes_per_minute_.load(std::memory_order_relaxed), 33);

  EXPECT_EQ(_scoped_entry2.metrics_->reads_.load(std::memory_order_relaxed), 5);
  EXPECT_EQ(_scoped_entry2.metrics_->reads_accumulator_.load(std::memory_order_relaxed), 11);
  EXPECT_EQ(_scoped_entry2.metrics_->writes_.load(std::memory_order_relaxed), 15);
  EXPECT_EQ(_scoped_entry2.metrics_->writes_accumulator_.load(std::memory_order_relaxed), 17);
  EXPECT_EQ(_scoped_entry2.metrics_->writes_per_minute_.load(std::memory_order_relaxed), 19);
#endif
}