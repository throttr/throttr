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

#include <throttr/message.hpp>
#include <throttr/messages_pool.hpp>

namespace throttr
{
  thread_local std::vector<std::shared_ptr<message>> messages_pool::available_;

  thread_local std::vector<std::shared_ptr<message>> messages_pool::used_;

  void messages_pool::prepares(const std::size_t initial)
  {
    available_.reserve(initial);
    for (std::size_t _e = 0; _e < initial; ++_e)
    {
      auto _message = std::make_shared<message>();
      _message->recyclable_ = true;
      available_.push_back(_message);
    }
  }

  void messages_pool::recycle()
  {
    for (auto it = used_.begin(); it != used_.end();)
    {
      if (!(*it)->in_use_)
      {
        (*it)->buffers_.clear();
        (*it)->buffers_.shrink_to_fit();
        (*it)->write_buffer_.clear();
        (*it)->write_buffer_.shrink_to_fit();
        available_.push_back(*it);
        it = used_.erase(it);
      }
      else
        ++it;
    }
  }

  void messages_pool::fit(const std::size_t count)
  {
    if (available_.size() > count)
    {
      available_.erase(available_.begin() + count, available_.end());
      available_.shrink_to_fit();
    }
  }

  std::shared_ptr<message> messages_pool::take_one(const std::size_t count)
  {
    recycle();
    fit();

    const bool _should_refill = available_.size() == 0;

    while (_should_refill && available_.size() < count)
    {
      const auto _scoped_message = std::make_shared<message>();
      _scoped_message->recyclable_ = true;
      available_.push_back(_scoped_message);
    }

    const auto _message = available_.front();
    _message->in_use_ = true;
    used_.push_back(_message);
    available_.erase(available_.begin());

    return _message;
  }
} // namespace throttr