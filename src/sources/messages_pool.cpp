#include <throttr/message.hpp>
#include <throttr/messages_pool.hpp>

namespace throttr
{
  thread_local std::vector<std::shared_ptr<message>> messages_pool::available_;

  thread_local std::vector<std::shared_ptr<message>> messages_pool::used_;

  void messages_pool::prepares(const std::size_t initial)
  {
    available_.reserve(initial);
    for (auto _e = 0; _e < initial; ++_e)
    {
      available_.push_back(std::make_shared<message>());
    }
  }

  void messages_pool::recycle()
  {
    for (auto it = used_.begin(); it != used_.end();)
    {
      if ((*it)->used_)
      {
        (*it)->used_ = false;
        available_.push_back(std::move(*it));
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