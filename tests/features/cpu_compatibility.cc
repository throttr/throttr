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
#include <throttr/protocol_wrapper.hpp>

TEST(CPUCompatibility, VerifyAlign)
{
  std::vector<std::byte> _vec(sizeof(value_type));
  ASSERT_EQ(_vec.size(), sizeof(value_type));
  void *_raw_ptr = _vec.data();
  const auto _address = reinterpret_cast<std::uintptr_t>(_raw_ptr);
  constexpr std::size_t _alignment = alignof(std::atomic<value_type>);
  ASSERT_EQ(_address % _alignment, 0) << "vec.data() isn't aligned to " << _alignment;
  auto *_atomic_ptr = reinterpret_cast<std::atomic<value_type> *>(_vec.data());
  _atomic_ptr->store(33, std::memory_order_relaxed);
  EXPECT_EQ(_atomic_ptr->load(std::memory_order_relaxed), 33);
}

TEST(CPUCompatibility, AtomicAlignmentForAllTypes)
{
  auto _check_align = [](std::size_t size, std::size_t alignment)
  {
    std::vector<std::byte> _vec(size);
    const std::uintptr_t _addr = reinterpret_cast<std::uintptr_t>(_vec.data());
    EXPECT_EQ(_addr % alignment, 0) << "Buffer isn't aligned to " << alignment;
  };

  _check_align(sizeof(std::uint8_t), alignof(std::atomic<std::uint8_t>));
  _check_align(sizeof(std::uint16_t), alignof(std::atomic<std::uint16_t>));
  _check_align(sizeof(std::uint32_t), alignof(std::atomic<std::uint32_t>));
  _check_align(sizeof(std::uint64_t), alignof(std::atomic<std::uint64_t>));
}
