#ifndef THROTTR_ENTRY_HPP
#define THROTTR_ENTRY_HPP

#include <throttr/protocol_wrapper.hpp>

namespace throttr {
    struct value_view {
        char* pointer_;
        std::size_t size_;
    };

    struct value_owned {
        std::unique_ptr<char[]> data_;
        std::size_t size_;

        value_owned() : data_(nullptr), size_(0) {}

        explicit value_owned(const std::size_t sz)
            : data_(std::make_unique<char[]>(sz)), size_(sz) {}

        value_owned(const char* src, const std::size_t sz)
            : data_(std::make_unique<char[]>(sz)), size_(sz) {
            std::memcpy(data_.get(), src, sz);
        }

        value_view view() const {
            return { data_.get(), size_ };
        }
    };

    struct entry {
        entry_types type_;
        value_owned value_;
        ttl_types ttl_type_;
        std::chrono::steady_clock::time_point expires_at_;
    };
}

#endif // THROTTR_ENTRY_HPP
