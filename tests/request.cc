#include <gtest/gtest.h>
#include <throttr/request.hpp>
#include <chrono>
#include <thread>
#include <vector>

using namespace throttr;

using namespace std::chrono;

std::vector<std::byte> build_request_buffer(
    const uint8_t ip_version,
    const std::vector<uint8_t>& ip_bytes,
    const uint16_t port,
    const uint32_t max_requests,
    const uint32_t ttl,
    const std::string_view url
) {
    std::vector<std::byte> _buffer;
    _buffer.push_back(static_cast<std::byte>(ip_version));
    _buffer.resize(_buffer.size() + 16, static_cast<std::byte>(0));
    for (size_t _i = 0; _i < ip_bytes.size(); ++_i) {
        _buffer[1 + _i] = static_cast<std::byte>(ip_bytes[_i]);
    }

    _buffer.push_back(static_cast<std::byte>(port & 0xFF));
    _buffer.push_back(static_cast<std::byte>(port >> 8 & 0xFF));

    _buffer.push_back(static_cast<std::byte>(max_requests & 0xFF));
    _buffer.push_back(static_cast<std::byte>(max_requests >> 8 & 0xFF));
    _buffer.push_back(static_cast<std::byte>(max_requests >> 16 & 0xFF));
    _buffer.push_back(static_cast<std::byte>(max_requests >> 24 & 0xFF));

    _buffer.push_back(static_cast<std::byte>(ttl & 0xFF));
    _buffer.push_back(static_cast<std::byte>(ttl >> 8 & 0xFF));
    _buffer.push_back(static_cast<std::byte>(ttl >> 16 & 0xFF));
    _buffer.push_back(static_cast<std::byte>(ttl >> 24 & 0xFF));

    _buffer.push_back(static_cast<std::byte>(url.size()));

    for (char _c : url) {
        _buffer.push_back(static_cast<std::byte>(_c));
    }

    return _buffer;
}

TEST(RequestViewTest, ParseIPv4) {
    auto _buffer = build_request_buffer(4, {127, 0, 0, 1}, 8080, 5, 60000, "/api/user");

    const auto _view = request_view::from_buffer(_buffer);

    EXPECT_EQ(_view.header_->ip_version_, 4);
    EXPECT_EQ(_view.header_->port_, 8080);
    EXPECT_EQ(_view.header_->max_requests_, 5);
    EXPECT_EQ(_view.header_->ttl_, 60000);
    EXPECT_EQ(_view.url_, "/api/user");

    auto _out = _view.to_buffer();
    ASSERT_EQ(_out.size(), _buffer.size());
    ASSERT_TRUE(std::equal(_out.begin(), _out.end(), _buffer.begin()));
}

TEST(RequestViewTest, ParseIPv6) {
    const std::vector<uint8_t> _ipv6 = {0x20, 0x01, 0x0d, 0xb8, 0x85, 0xa3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    auto _buffer = build_request_buffer(6, _ipv6, 443, 10, 120000, "/api/auth");

    const auto _view = request_view::from_buffer(_buffer);

    EXPECT_EQ(_view.header_->ip_version_, 6);
    EXPECT_EQ(_view.header_->port_, 443);
    EXPECT_EQ(_view.header_->max_requests_, 10);
    EXPECT_EQ(_view.header_->ttl_, 120000);
    EXPECT_EQ(_view.url_, "/api/auth");

    auto _out = _view.to_buffer();
    ASSERT_EQ(_out.size(), _buffer.size());
    ASSERT_TRUE(std::equal(_out.begin(), _out.end(), _buffer.begin()));
}

TEST(RequestViewTest, RejectsTooSmallBuffer) {
    std::vector _buffer(5, static_cast<std::byte>(0));
    ASSERT_THROW(request_view::from_buffer(_buffer), std::runtime_error);
}

TEST(RequestViewTest, RejectsMissingUrlPayload) {
    auto _buffer = build_request_buffer(4, {127, 0, 0, 1}, 8080, 2, 5000, "/endpoint");

    _buffer[sizeof(request_header) - 1] = std::byte{100};

    ASSERT_THROW(request_view::from_buffer(_buffer), std::runtime_error);
}

TEST(RequestViewTest, ToBufferMatchesOriginal) {
    auto _buffer = build_request_buffer(4, {10, 0, 0, 1}, 1234, 15, 3000, "/endpoint");

    const auto _view = request_view::from_buffer(_buffer);

    auto _reconstructed = _view.to_buffer();
    ASSERT_EQ(_reconstructed.size(), _buffer.size());
    ASSERT_TRUE(std::equal(_reconstructed.begin(), _reconstructed.end(), _buffer.begin()));
}

TEST(RequestViewBenchmark, DecodePerformance) {
    auto _buffer = build_request_buffer(
        4, {127, 0, 0, 1}, 8080, 5, 60000, "/api/benchmark"
    );

    constexpr size_t _iterations = 1'000'000;
    const auto _start = high_resolution_clock::now();

    for (size_t _i = 0; _i < _iterations; ++_i) {
        auto _view = request_view::from_buffer(_buffer);
        EXPECT_EQ(_view.header_->ip_version_, 4);
    }

    const auto _end = high_resolution_clock::now();
    const auto _duration = duration_cast<milliseconds>(_end - _start);

    std::cout << "iterations: " << _iterations
              << " on " << _duration.count() << " ms" << std::endl;
}

TEST(RequestViewBenchmark, MultiThreadedDecodePerformance) {
    constexpr size_t _num_threads = 2;
    std::atomic _start_flag{false};
    std::atomic<size_t> _completed_threads{0};

    std::vector<std::jthread> _threads;
    _threads.reserve(_num_threads);

    for (size_t _t = 0; _t < _num_threads; ++_t) {
        _threads.emplace_back([&_start_flag, &_completed_threads] {
            const auto _buffer = build_request_buffer(4, {127, 0, 0, 1}, 8080, 5, 60000, "/api/test");
            const auto _span = std::span(
                _buffer.data(), _buffer.size()
            );
            constexpr size_t _decodes_per_thread = 1'000'000;

            while (!_start_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (size_t _i = 0; _i < _decodes_per_thread; ++_i) {
                request_view::from_buffer(_span);
            }
            ++_completed_threads;
        });
    }

    const auto _start = high_resolution_clock::now();
    _start_flag.store(true, std::memory_order_release);

    while (_completed_threads.load(std::memory_order_acquire) < _num_threads) {
        std::this_thread::yield();
    }

    const auto _end = high_resolution_clock::now();
    const auto _elapsed = std::chrono::duration_cast<nanoseconds>(_end - _start).count();

    constexpr size_t _total_decodes = _num_threads * 1'000'000;

    const auto _decode_rate = _total_decodes * 1000000000ULL / _elapsed;

    std::cout << "Decodes: " << _total_decodes
              << " in " << _elapsed << " ns, "
              << _decode_rate << " decode/s" << std::endl;
}

TEST(RequestViewTest, ParseRequestWithZeroMaxRequests) {
    auto _buffer = build_request_buffer(4, {127, 0, 0, 1}, 8080, 0, 60000, "/zero");

    const auto _view = request_view::from_buffer(_buffer);

    EXPECT_EQ(_view.header_->ip_version_, 4);
    EXPECT_EQ(_view.header_->max_requests_, 0);
    EXPECT_EQ(_view.url_, "/zero");

    auto _out = _view.to_buffer();
    ASSERT_EQ(_out.size(), _buffer.size());
    ASSERT_TRUE(std::equal(_out.begin(), _out.end(), _buffer.begin()));
}

TEST(RequestViewTest, ParseRequestWithMaxUInt32MaxRequests) {
    auto _buffer = build_request_buffer(4, {192, 168, 1, 1}, 8080, 0xFFFFFFFF, 60000, "/max-requests");

    const auto _view = request_view::from_buffer(_buffer);

    EXPECT_EQ(_view.header_->ip_version_, 4);
    EXPECT_EQ(_view.header_->max_requests_, 0xFFFFFFFF);
    EXPECT_EQ(_view.url_, "/max-requests");

    auto _out = _view.to_buffer();
    ASSERT_EQ(_out.size(), _buffer.size());
    ASSERT_TRUE(std::equal(_out.begin(), _out.end(), _buffer.begin()));
}

TEST(RequestViewTest, ParseRequestWithZeroTTL) {
    auto _buffer = build_request_buffer(4, {127, 0, 0, 1}, 8080, 5, 0, "/zero-ttl");

    const auto _view = request_view::from_buffer(_buffer);

    EXPECT_EQ(_view.header_->ttl_, 0);
    EXPECT_EQ(_view.url_, "/zero-ttl");

    auto _out = _view.to_buffer();
    ASSERT_EQ(_out.size(), _buffer.size());
    ASSERT_TRUE(std::equal(_out.begin(), _out.end(), _buffer.begin()));
}

TEST(RequestViewTest, ParseRequestWithMaxTTL) {
    auto _buffer = build_request_buffer(4, {127, 0, 0, 1}, 8080, 5, 0xFFFFFFFF, "/max-ttl");

    const auto _view = request_view::from_buffer(_buffer);

    EXPECT_EQ(_view.header_->ttl_, 0xFFFFFFFF);
    EXPECT_EQ(_view.url_, "/max-ttl");

    auto _out = _view.to_buffer();
    ASSERT_EQ(_out.size(), _buffer.size());
    ASSERT_TRUE(std::equal(_out.begin(), _out.end(), _buffer.begin()));
}

TEST(RequestViewTest, ParseRequestWithEmptyURL) {
    auto _buffer = build_request_buffer(4, {127, 0, 0, 1}, 8080, 5, 60000, "");

    const auto _view = request_view::from_buffer(_buffer);

    EXPECT_EQ(_view.url_, "");
    EXPECT_EQ(_view.header_->size_, 0);

    auto _out = _view.to_buffer();
    ASSERT_EQ(_out.size(), sizeof(request_header));
    ASSERT_TRUE(std::equal(_out.begin(), _out.end(), _buffer.begin()));
}

TEST(RequestViewTest, ParseRequestWithMaxSizeURL) {
    const std::string _max_url(255, 'a');
    auto _buffer = build_request_buffer(4, {10, 0, 0, 1}, 8080, 5, 60000, _max_url);

    const auto _view = request_view::from_buffer(_buffer);

    EXPECT_EQ(_view.header_->size_, 255);
    EXPECT_EQ(_view.url_.size(), 255);
    EXPECT_TRUE(std::all_of(_view.url_.begin(), _view.url_.end(), [](char _c) { return _c == 'a'; }));

    auto _out = _view.to_buffer();
    ASSERT_EQ(_out.size(), sizeof(request_header) + 255);
    ASSERT_TRUE(std::equal(_out.begin(), _out.end(), _buffer.begin()));
}

TEST(RequestViewTest, ToBufferPreservesHeaderAndPayload) {
    auto _buffer = build_request_buffer(4, {192, 168, 0, 1}, 1234, 100, 10000, "/preserve");

    const auto _view = request_view::from_buffer(_buffer);
    auto _reconstructed = _view.to_buffer();

    ASSERT_EQ(_reconstructed.size(), _buffer.size());
    ASSERT_TRUE(std::equal(_reconstructed.begin(), _reconstructed.end(), _buffer.begin()));
}

TEST(RequestViewTest, RejectsInvalidIPVersion) {
    auto _buffer = build_request_buffer(7, {127, 0, 0, 1}, 8080, 5, 60000, "/invalid-ip");

    const auto _view = request_view::from_buffer(_buffer);

    EXPECT_NE(_view.header_->ip_version_, 4);
    EXPECT_NE(_view.header_->ip_version_, 6);
}

TEST(RequestViewTest, RejectsCorruptHeaderOverflowSize) {
    auto _buffer = build_request_buffer(4, {127, 0, 0, 1}, 8080, 5, 60000, "/short");

    auto* _raw = reinterpret_cast<uint8_t*>(_buffer.data());
    _raw[sizeof(request_header) - 1] = 250;

    ASSERT_THROW(request_view::from_buffer(_buffer), std::runtime_error);
}

TEST(RequestViewBenchmark, PerformanceBenchmarkExtremeRequests) {
    auto _buffer = build_request_buffer(
        4, {127, 0, 0, 1}, 8080, 0xFFFFFFFF, 0xFFFFFFFF, "/extreme"
    );

    constexpr size_t _iterations = 1'000'000;
    const auto _start = high_resolution_clock::now();

    for (size_t _i = 0; _i < _iterations; ++_i) {
        auto _view = request_view::from_buffer(_buffer);
        EXPECT_EQ(_view.header_->max_requests_, 0xFFFFFFFF);
    }

    const auto _end = high_resolution_clock::now();
    const auto _duration = duration_cast<milliseconds>(_end - _start);

    std::cout << "Extreme iterations: " << _iterations
              << " in " << _duration.count() << " ms" << std::endl;
}
