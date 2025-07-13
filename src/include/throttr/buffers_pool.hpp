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

#ifndef THROTTR__BUFFERS_POOL_HPP
#define THROTTR__BUFFERS_POOL_HPP

#include <vector>

#include <throttr/reusable_buffer.hpp>

namespace throttr
{
  /**
   * Buffers pool
   */
  class buffers_pool
  {
  public:
    /**
     * Available
     */
    static thread_local std::vector<std::shared_ptr<reusable_buffer>> available_;

    /**
     * Used
     */
    static thread_local std::vector<std::shared_ptr<reusable_buffer>> used_;

    /**
     * Prepares
     */
    static void prepares(std::size_t initial = 16);

    /**
     * Recycle
     */
    static void recycle();

    /**
     * Take one
     *
     * @return
     */
    static std::shared_ptr<reusable_buffer> take_one(std::size_t count = 16);
  };
} // namespace throttr

#endif // THROTTR__BUFFERS_POOL_HPP
