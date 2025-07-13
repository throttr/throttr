#include <throttr/state.hpp>

#include <throttr/buffers_pool.hpp>

namespace throttr
{
  thread_local std::vector<std::shared_ptr<reusable_buffer>> buffers_pool::available_;

  thread_local std::vector<std::shared_ptr<reusable_buffer>> buffers_pool::used_;

  void buffers_pool::prepares(const std::size_t initial)
  {
    available_.reserve(initial);
    for (std::size_t _e = 0; _e < initial; ++_e)
    {
      available_.push_back(std::make_shared<reusable_buffer>());
    }
  }

  void buffers_pool::recycle()
  {
    std::size_t recycled = 0;
    for (auto it = used_.begin(); it != used_.end();)
    {
      if ((*it)->can_be_reused_)
      {
        (*it)->can_be_reused_ = false;
        const auto _scoped_buffer = (*it)->buffer_.load(std::memory_order_relaxed);
        _scoped_buffer->clear();
        _scoped_buffer->shrink_to_fit();
        it = used_.erase(it);
        recycled++;
      }
      else
        ++it;
    }
  }

  std::shared_ptr<reusable_buffer> buffers_pool::take_one(const std::size_t count)
  {
    while (available_.size() < count)
    {
      available_.push_back(std::make_shared<reusable_buffer>());
    }

    const auto _element = available_.front();
    available_.erase(available_.begin());

    used_.push_back(_element);

    return _element;
  }
} // namespace throttr