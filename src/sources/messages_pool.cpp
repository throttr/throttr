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

#include <algorithm>
#include <throttr/message.hpp>
#include <throttr/messages_pool.hpp>

namespace throttr
{
  thread_local boost::circular_buffer<std::shared_ptr<message>> messages_pool::available_;

  thread_local boost::circular_buffer<std::shared_ptr<message>> messages_pool::used_;

  void messages_pool::prepares(const std::size_t initial)
  {
    available_.set_capacity(initial * 2);
    used_.set_capacity(initial * 2);
    for (std::size_t _e = 0; _e < initial; ++_e)
    {
      auto _message = std::make_shared<message>();
      _message->recyclable_ = true;
      available_.push_back(_message);
    }
  }

  void messages_pool::recycle()
  {
    const auto _end = std::remove_if(
      used_.begin(),
      used_.end(),
      [&](const auto &msg)
      {
        if (!msg->in_use_)
        {
          msg->buffers_.clear();
          msg->buffers_.shrink_to_fit();
          msg->write_buffer_.clear();
          msg->write_buffer_.shrink_to_fit();
          available_.push_back(msg);
          return true;
        }
        return false;
      });

    used_.erase(_end, used_.end());
  }

  void messages_pool::fit(const std::size_t count)
  {
    if (available_.size() > count)
    {
      available_.erase(available_.begin() + count, available_.end());
    }
  }

  std::shared_ptr<message> messages_pool::take_one(const std::size_t count)
  {
    recycle();
    fit();

    const bool _should_refill = available_.empty();

    while (_should_refill && available_.size() < count)
    {
      const auto _scoped_message = std::make_shared<message>();
      _scoped_message->recyclable_ = true;
      available_.push_back(_scoped_message);
    }

    const auto _message = available_.back();
    _message->in_use_ = true;
    used_.push_back(_message);

    available_.pop_back();

    return _message;
  }
} // namespace throttr