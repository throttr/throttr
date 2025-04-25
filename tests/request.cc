#include <gtest/gtest.h>
#include <throttr/request.hpp>

using namespace throttr;

auto build_request_buffer(
    const uint8_t ip_version,
    const std::vector<uint8_t> &ip_bytes,
    const uint16_t port,
    const std::string &url,
    const uint8_t max_requests,
    const uint32_t ttl_ms
) -> std::vector<uint8_t> {
    std::vector<uint8_t> buffer;
    buffer.push_back(ip_version);
    buffer.insert(buffer.end(), ip_bytes.begin(), ip_bytes.end());
    buffer.push_back(static_cast<uint8_t>(port >> 8));
    buffer.push_back(static_cast<uint8_t>(port & 0xFF));
    buffer.push_back(static_cast<uint8_t>(url.size()));
    buffer.insert(buffer.end(), url.begin(), url.end());
    buffer.push_back(max_requests);
    buffer.push_back((ttl_ms >> 24) & 0xFF);
    buffer.push_back((ttl_ms >> 16) & 0xFF);
    buffer.push_back((ttl_ms >> 8) & 0xFF);
    buffer.push_back(ttl_ms & 0xFF);
    return buffer;
}

TEST(ProtocolTest, RequestFromStringToBytesSymmetry) {
    constexpr uint8_t raw_ipv4[] = {192, 168, 1, 100};
    constexpr uint16_t port = 8080;
    std::string url = "/api/user";
    constexpr uint8_t max = 10;
    constexpr uint32_t ttl = 60000;

    const auto buffer = build_request_buffer(4, {192, 168, 1, 100}, 8080, "/api/user", 10, 60000);
    const auto req = request::from_string(std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size()));
    const auto serialized = req.to_bytes();

    ASSERT_EQ(serialized.size(), buffer.size());
    for (size_t i = 0; i < buffer.size(); ++i) {
        ASSERT_EQ(serialized[i], buffer[i]) << "Mismatch at byte " << i;
    }
}


TEST(ProtocolTest, RequestIPv6Support) {
    const std::array<uint8_t, 16> ipv6 = {0x20,0x01,0x0d,0xb8,0x85,0xa3,0x00,0x00,0x00,0x00,0x8a,0x2e,0x03,0x70,0x73,0x34};
    constexpr uint16_t port = 443;
    std::string url = "/ipv6/test";
    constexpr uint8_t max = 7;
    constexpr uint32_t ttl = 12345;

    const auto buffer = build_request_buffer(4, {192, 168, 1, 100}, 8080, "/api/user", 10, 60000);
    const auto req = request::from_string(std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size()));
    const auto serialized = req.to_bytes();

    ASSERT_EQ(serialized, buffer);
}


TEST(ProtocolTest, RejectsInvalidIPVersion) {
    const std::vector<uint8_t> buffer = {0x01};
    ASSERT_THROW(request::from_string(std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size())), std::runtime_error);
}

TEST(ProtocolTest, RejectsTruncatedIPv4) {
    const std::vector<uint8_t> buffer = {4, 192, 168};
    ASSERT_THROW(request::from_string(std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size())), std::runtime_error);
}

TEST(ProtocolTest, RejectsTruncatedPort) {
    const std::vector<uint8_t> buffer = {4, 127,0,0,1, 0x1F};
    ASSERT_THROW(request::from_string(std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size())), std::runtime_error);
}

TEST(ProtocolTest, RejectsTruncatedURL) {
    const std::vector<uint8_t> buffer = {4, 127,0,0,1, 0x1F, 0x90, 3, 'a'};
    ASSERT_THROW(request::from_string(std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size())), std::runtime_error);
}


TEST(ProtocolTest, RejectsMissingMaxRequests) {
    const std::vector<uint8_t> buffer = {4, 127,0,0,1, 0x1F, 0x90, 3, 'a', 'b', 'c'};
    ASSERT_THROW(request::from_string(std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size())), std::runtime_error);
}

TEST(ProtocolTest, RejectsMissingTTL) {
    const std::vector<uint8_t> buffer = {4, 127,0,0,1, 0x1F, 0x90, 3, 'a', 'b', 'c', 5};
    ASSERT_THROW(request::from_string(std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size())), std::runtime_error);
}

TEST(ProtocolTest, RejectsTooLongURL) {
    request req;
    req.ip = std::span<const uint8_t, 4>(std::array<uint8_t, 4>{1,2,3,4}.data(), 4);
    req.port = 1234;
    req.url = std::string_view("x", 300);
    req.max_requests = 42;
    req.ttl_ms = 1000;
    ASSERT_THROW(req.to_bytes(), std::runtime_error);
}

TEST(ProtocolTest, RejectsTruncatedIPv4Exactly) {
    const std::vector<uint8_t> buffer = {
        4, 192, 168, 0
    };
    ASSERT_THROW(request::from_string(std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size())), std::runtime_error);
}

TEST(ProtocolTest, RejectsTruncatedPortExactly) {
    const std::vector<uint8_t> buffer = {
        4, 127, 0, 0, 1,
    };
    ASSERT_THROW(request::from_string(std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size())), std::runtime_error);
}

TEST(ProtocolTest, RejectsTruncatedPortWithOneByte) {
    const std::vector<uint8_t> buffer = {
        4, 127, 0, 0, 1,
        0x1F
    };
    ASSERT_THROW(request::from_string(std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size())), std::runtime_error);
}

TEST(ProtocolTest, RejectsURLOutOfBounds) {
    const std::vector<uint8_t> buffer = {
        4, 127, 0, 0, 1,
        0x1F, 0x90,
        5,
        'a', 'b'
    };
    ASSERT_THROW(request::from_string(std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size())), std::runtime_error);
}

TEST(ProtocolTest, RejectsMissingMaxRequestsExactly) {
    const std::vector<uint8_t> buffer = {
        4, 127, 0, 0, 1,
        0x1F, 0x90,
        3,
        'a', 'b', 'c'
    };
    ASSERT_THROW(request::from_string(std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size())), std::runtime_error);
}

TEST(ProtocolTest, RejectsTruncatedTTL) {
    const std::vector<uint8_t> buffer = {
        4, 127, 0, 0, 1,
        0x1F, 0x90,
        3, 'a', 'b', 'c',
        5,
        0x00, 0x00
    };
    ASSERT_THROW(request::from_string(std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size())), std::runtime_error);
}

TEST(RequestValidationTest, RejectsBufferTooSmall) {
    constexpr std::vector<uint8_t> buffer = {};
    ASSERT_THROW(throttr::request::from_string(
        std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size())),
        std::runtime_error);
}

TEST(RequestValidationTest, RejectsInvalidIPv6Buffer) {
    std::vector<uint8_t> buffer = {6};
    buffer.resize(10, 0x00);
    ASSERT_THROW(throttr::request::from_string(
        std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size())),
        std::runtime_error);
}

TEST(RequestValidationTest, RejectsMissingUrlLength) {
    const std::vector<uint8_t> buffer = {
        4, 127, 0, 0, 1,
        0x1F, 0x90
    };
    ASSERT_THROW(throttr::request::from_string(
        std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size())),
        std::runtime_error);
}

TEST(RequestValidationTest, RejectsToBytesWhenIPNotInitialized) {
    request req;

    req.port = 8080;
    req.url = "/test";
    req.max_requests = 5;
    req.ttl_ms = 10000;

    ASSERT_THROW(req.to_bytes(), std::runtime_error);
}