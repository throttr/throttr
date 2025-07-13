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
    static void prepares(std::size_t initial = 64);

    /**
     * Recycle
     */
    static void recycle();

    /**
     * Take one
     *
     * @return
     */
    static std::shared_ptr<reusable_buffer> take_one(std::size_t count = 1024);
  };
} // namespace throttr

#endif // THROTTR__BUFFERS_POOL_HPP
