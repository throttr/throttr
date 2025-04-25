#include <gtest/gtest.h>
#include <throttr/request.hpp>
#include <chrono>
#include <thread>
#include <vector>

using namespace throttr;

using namespace std::chrono;

std::vector<std::byte> build_request_buffer(
    const uint8_t ip_version,
    const std::vector<uint8_t> &ip_bytes,
    const uint16_t port,
    const uint8_t max_requests,
    const uint32_t ttl,
    const std::string_view url
) {
    std::vector<std::byte> buffer;
    buffer.push_back(static_cast<std::byte>(ip_version));
    buffer.resize(buffer.size() + 16, static_cast<std::byte>(0));
    for (size_t i = 0; i < ip_bytes.size(); ++i) {
        buffer[1 + i] = static_cast<std::byte>(ip_bytes[i]);
    }

    buffer.push_back(static_cast<std::byte>(port & 0xFF));
    buffer.push_back(static_cast<std::byte>((port >> 8) & 0xFF));

    buffer.push_back(static_cast<std::byte>(max_requests));

    buffer.push_back(static_cast<std::byte>(ttl & 0xFF));
    buffer.push_back(static_cast<std::byte>((ttl >> 8) & 0xFF));
    buffer.push_back(static_cast<std::byte>((ttl >> 16) & 0xFF));
    buffer.push_back(static_cast<std::byte>((ttl >> 24) & 0xFF));

    buffer.push_back(static_cast<std::byte>(url.size()));

    for (char c: url) {
        buffer.push_back(static_cast<std::byte>(c));
    }

    return buffer;
}

TEST(RequestViewTest, ParseIPv4) {
    auto buffer = build_request_buffer(4, {127, 0, 0, 1}, 8080, 5, 60000, "/api/user");

    const auto view = request_view::from_buffer(buffer);

    EXPECT_EQ(view.header_->ip_version_, 4);
    EXPECT_EQ(view.header_->port_, 8080);
    EXPECT_EQ(view.header_->max_requests_, 5);
    EXPECT_EQ(view.header_->ttl_, 60000);
    EXPECT_EQ(view.url_, "/api/user");

    auto out = view.to_buffer();
    ASSERT_EQ(out.size(), buffer.size());
    ASSERT_TRUE(std::equal(out.begin(), out.end(), buffer.begin()));
}

TEST(RequestViewTest, ParseIPv6) {
    const std::vector<uint8_t> ipv6 = {0x20, 0x01, 0x0d, 0xb8, 0x85, 0xa3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    auto buffer = build_request_buffer(6, ipv6, 443, 10, 120000, "/api/auth");

    const auto view = request_view::from_buffer(buffer);

    EXPECT_EQ(view.header_->ip_version_, 6);
    EXPECT_EQ(view.header_->port_, 443);
    EXPECT_EQ(view.header_->max_requests_, 10);
    EXPECT_EQ(view.header_->ttl_, 120000);
    EXPECT_EQ(view.url_, "/api/auth");

    auto out = view.to_buffer();
    ASSERT_EQ(out.size(), buffer.size());
    ASSERT_TRUE(std::equal(out.begin(), out.end(), buffer.begin()));
}

TEST(RequestViewTest, RejectsTooSmallBuffer) {
    std::vector buffer(5, static_cast<std::byte>(0));
    ASSERT_THROW(request_view::from_buffer(buffer), std::runtime_error);
}

TEST(RequestViewTest, RejectsMissingUrlPayload) {
    auto buffer = build_request_buffer(4, {127, 0, 0, 1}, 8080, 2, 5000, "/endpoint");

    buffer[sizeof(request_header) - 1] = std::byte{100};

    ASSERT_THROW(request_view::from_buffer(buffer), std::runtime_error);
}

TEST(RequestViewTest, ToBufferMatchesOriginal) {
    auto buffer = build_request_buffer(4, {10, 0, 0, 1}, 1234, 15, 3000, "/endpoint");

    const auto view = request_view::from_buffer(buffer);

    auto reconstructed = view.to_buffer();
    ASSERT_EQ(reconstructed.size(), buffer.size());
    ASSERT_TRUE(std::equal(reconstructed.begin(), reconstructed.end(), buffer.begin()));
}

TEST(RequestViewBenchmark, DecodePerformance) {
    auto buffer = build_request_buffer(
        4, {127, 0, 0, 1}, 8080, 5, 60000, "/api/benchmark"
    );

    constexpr size_t iterations = 1'000'000;
    const auto start = high_resolution_clock::now();

    for (size_t i = 0; i < iterations; ++i) {
        auto view = request_view::from_buffer(buffer);
        EXPECT_EQ(view.header_->ip_version_, 4);
    }

    const auto end = high_resolution_clock::now();
    const auto duration = duration_cast<milliseconds>(end - start);

    std::cout << "iterations: " << iterations
            << " on " << duration.count() << " ms" << std::endl;
}

TEST(RequestViewBenchmark, MultiThreadedDecodePerformance) {
    constexpr size_t num_threads = 2;
    std::atomic start_flag{false};
    std::atomic<size_t> completed_threads{0};

    std::vector<std::jthread> threads;
    threads.reserve(num_threads);

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&start_flag, &completed_threads] {
            const auto buffer = build_request_buffer(4, {127, 0, 0, 1}, 8080, 5, 60000, "/api/test");
            const auto span = std::span(
                buffer.data(), buffer.size()
            );
            constexpr size_t decodes_per_thread = 1'000'000;

            while (!start_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (size_t i = 0; i < decodes_per_thread; ++i) {
                request_view::from_buffer(span);
            }
            ++completed_threads;
        });
    }

    const auto start = high_resolution_clock::now();
    start_flag.store(true, std::memory_order_release);

    while (completed_threads.load(std::memory_order_acquire) < num_threads) {
        std::this_thread::yield();
    }

    const auto end = high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast<nanoseconds>(end - start).count();


    constexpr size_t total_decodes = num_threads * 1'000'000;

    const auto decode_rate = (total_decodes * 1000000000ull) / elapsed;

    std::cout << "Decodes: " << total_decodes
            << " in " << elapsed << " ns, "
            << decode_rate << " decode/s" << std::endl;
}
