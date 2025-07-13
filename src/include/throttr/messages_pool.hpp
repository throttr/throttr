#ifndef THROTTR__MESSAGES_POOL_HPP
#define THROTTR__MESSAGES_POOL_HPP

#include <memory>
#include <vector>

namespace throttr
{
  class message;

  /**
   * Messages pool
   */
  class messages_pool
  {
  public:
    /**
     * Available
     */
    static thread_local std::vector<std::shared_ptr<message>> available_;

    /**
     * Used
     */
    static thread_local std::vector<std::shared_ptr<message>> used_;

    /**
     * Prepares
     */
    static void prepares(std::size_t initial = 1024);

    /**
     * Recycle
     */
    static void recycle();

    /**
     * Refill
     */
    static void fit(std::size_t count = 1024);

    /**
     * Grab
     *
     * @return
     */
    static std::shared_ptr<message> take_one(std::size_t count = 1024);
  };
} // namespace throttr

#endif // THROTTR__MESSAGES_POOL_HPP
