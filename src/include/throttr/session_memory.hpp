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

#pragma once

#ifndef THROTTR_SESSION_MEMORY_HPP
#define THROTTR_SESSION_MEMORY_HPP
#include <cstddef>

namespace throttr {
    // LCOV_EXCL_START Note: This isn't something that i could cover

    /**
     * Handler memory
     */
    class handler_memory {
    public:
        /**
         * Constructor
         */
        handler_memory() {}

        /**
         * Swap operator
         */
        handler_memory(const handler_memory&) = delete;

        /**
         * Assignment operator
         *
         * @return handler_memory
         */
        handler_memory& operator=(const handler_memory&) = delete;

        /**
         * Allocate
         *
         * @param size
         * @return void*
         */
        void* allocate(const std::size_t size) { // NOSONAR
            if (!in_use_ && size < sizeof(storage_)) {
                in_use_ = true;
                return &storage_;
            }
            return ::operator new(size);
        }

        /**
         * Deallocate
         *
         * @param pointer
         */
        void deallocate(void* pointer) { // NOSONAR
            if (pointer == &storage_) {
                in_use_ = false;
            } else {
                ::operator delete(pointer);
            }
        }

    private:
        // Note: That 16 should be adjusted if we pass more
        // captured objects into the read and write lambdas

        /**
         * Storage
         */
        typename std::aligned_storage<16>::type storage_; // NOSONAR

        /**
         * In use
         */
        bool in_use_ = false;
    };

    /**
     * Handler allocator
     */
    template <typename T>
    class handler_allocator {
    public:
        using value_type = T;

        /**
         * Constructor
         *
         * @param mem
         */
        explicit handler_allocator(handler_memory& mem) noexcept : memory_(mem) {}

        /**
         * Swap operator
         *
         * @tparam U
         * @param other
         */
        template <typename U>
        handler_allocator(const handler_allocator<U>& other) noexcept : memory_(other.memory_) {}  // NOSONAR

        /**
         * Allocate
         *
         * @param n
         * @return
         */
        T* allocate(const std::size_t n) const {
            return static_cast<T*>(memory_.allocate(sizeof(T) * n));
        }

        /**
         * Deallocate
         *
         * @param p
         */
        void deallocate(T* p, std::size_t) const {
            memory_.deallocate(p);
        }

        /**
         * Equals operator
         *
         * @tparam U
         * @param other
         * @return bool
         */
        template <typename U>
        bool operator==(const handler_allocator<U>& other) const noexcept {
            return &memory_ == &other.memory_;
        }

        /**
         * Not equals operator
         *
         * @tparam U
         * @param other
         * @return bool
         */
        template <typename U>
        bool operator!=(const handler_allocator<U>& other) const noexcept {
            return !(*this == other);
        }

    private:
        /**
         * Memory
         *
         * @tparam
         */
        template <typename> friend class handler_allocator;
        handler_memory& memory_;
    };

    // LCOV_EXCL_STOP
}

#endif // THROTTR_SESSION_MEMORY_HPP
