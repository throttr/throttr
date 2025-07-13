#ifndef THROTTR__REUSABLE_BUFFER_HPP
#define THROTTR__REUSABLE_BUFFER_HPP

#include <atomic>
#include <iostream>
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
