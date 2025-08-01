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

#include <throttr/state.hpp>

#include <throttr/buffers_pool.hpp>

namespace throttr
{
  thread_local boost::circular_buffer<std::shared_ptr<reusable_buffer>> buffers_pool::available_;

  thread_local boost::circular_buffer<std::shared_ptr<reusable_buffer>> buffers_pool::used_;

  void buffers_pool::prepares(const std::size_t initial)
  {
    available_.set_capacity(initial * 2);
    used_.set_capacity(initial * 2);
    for (std::size_t _e = 0; _e < initial; ++_e)
    {
      auto _reusable_buffer = std::make_shared<reusable_buffer>();
      _reusable_buffer->recyclable_ = true;
      available_.push_back(_reusable_buffer);
    }
  }

  void buffers_pool::recycle()
  {
    auto _end = std::remove_if(
      used_.begin(),
      used_.end(),
      [&](const auto &buf)
      {
        if (!buf->in_use_)
        {
          const auto _scoped_buffer = buf->buffer_.load(std::memory_order_relaxed);
          _scoped_buffer->clear();
          _scoped_buffer->shrink_to_fit();
          available_.push_back(buf);
          return true;
        }
        return false;
      });

    used_.erase(_end, used_.end());
  }

  void buffers_pool::fit(const std::size_t count)
  {
    if (available_.size() > count)
    {
      available_.erase(available_.begin() + count, available_.end());
    }
  }

  std::shared_ptr<reusable_buffer> buffers_pool::take_one(const std::size_t count)
  {
    recycle();
    fit();

    const bool _should_refill = available_.empty();

    while (_should_refill && available_.size() < count)
    {
      auto _scoped_reusable_buffer = std::make_shared<reusable_buffer>();
      _scoped_reusable_buffer->recyclable_ = true;
      available_.push_back(_scoped_reusable_buffer);
    }

    const auto _reusable_buffer = available_.back();
    _reusable_buffer->in_use_ = true;
    used_.push_back(_reusable_buffer);

    available_.pop_back();

    return _reusable_buffer;
  }
} // namespace throttr