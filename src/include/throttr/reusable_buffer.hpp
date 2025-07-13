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

#ifndef THROTTR__REUSABLE_BUFFER_HPP
#define THROTTR__REUSABLE_BUFFER_HPP

#include <atomic>
#include <memory>
#include <vector>

namespace throttr
{
  /**
   * Reusable buffer
   */
  class reusable_buffer : public std::enable_shared_from_this<reusable_buffer>
  {
  public:
    /**
     * Can be reused
     */
    bool can_be_reused_ = false;

    /**
     * Buffer
     */
    std::atomic<std::shared_ptr<std::vector<std::byte>>> buffer_;

    /**
     * Constructor
     */
    reusable_buffer() : buffer_(std::make_shared<std::vector<std::byte>>()){};
  };
} // namespace throttr

#endif // THROTTR__REUSABLE_BUFFER_HPP
