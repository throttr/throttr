#include "fmt/ostream.h"

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
      available_.push_back(std::make_shared<message>());
    }
  }

  void messages_pool::recycle()
  {
    std::size_t recycled = 0;
    for (auto it = used_.begin(); it != used_.end();)
    {
      if ((*it)->used_)
      {
        (*it)->used_ = false;
        (*it)->write_buffer_.clear();
        (*it)->write_buffer_.shrink_to_fit();
        (*it)->buffers_.clear();
        (*it)->buffers_.shrink_to_fit();
        available_.push_back(std::move(*it));
        it = used_.erase(it);
        recycled++;
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
    while (available_.size() < count)
    {
      available_.push_back(std::make_shared<message>());
    }

    const auto _message = available_.front();
    available_.erase(available_.begin());

    _message->used_ = true;

    used_.push_back(_message);

    return _message;
  }
} // namespace throttr