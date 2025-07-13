#ifndef THROTTR__REUSABLE_BUFFER_HPP
#define THROTTR__REUSABLE_BUFFER_HPP

#include <atomic>
#include <memory>
#include <vector>

namespace throttr
{
  class reusable_buffer : public std::enable_shared_from_this<reusable_buffer>
  {
  public:
    bool can_be_reused_ = false;

    std::atomic<std::shared_ptr<std::vector<std::byte>>> buffer_;

    reusable_buffer() : buffer_(std::make_shared<std::vector<std::byte>>()){};
  };
} // namespace throttr

#endif // THROTTR__REUSABLE_BUFFER_HPP
