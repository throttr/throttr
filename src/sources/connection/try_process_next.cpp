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

#include <throttr/connection.hpp>

#include <throttr/message.hpp>
#include <throttr/state.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  void connection::try_process_next()
  {
    const auto _message = std::make_shared<message>();

    while (true)
    {
      const std::span<const std::byte> _span(buffer_.data() + buffer_start_, buffer_end_ - buffer_start_);
      const std::size_t _msg_size = get_message_size(_span);
      if (_msg_size == 0 || _span.size() < _msg_size) // LCOV_EXCL_LINE note: Ignored.
        break;

      const std::span<const std::byte> _view(buffer_.data() + buffer_start_, _msg_size);
      buffer_start_ += _msg_size;

      // LCOV_EXCL_START
#ifndef NDEBUG
      fmt::println(
        "{:%Y-%m-%d %H:%M:%S} SESSION READ ip={} port={} buffer={}",
        std::chrono::system_clock::now(),
        ip_,
        port_,
        span_to_hex(_view));
#endif
      // LCOV_EXCL_STOP

      const auto _type = static_cast<request_types>(std::to_integer<uint8_t>(_view[0]));
      state_->commands_->commands_[static_cast<std::size_t>(
        _type)](state_, _type, _view, _message->buffers_, _message->write_buffer_, shared_from_this());
      // LCOV_EXCL_STOP
    }

    // LCOV_EXCL_START Note: Partially tested.
    // The not tested case is when in-while break condition is triggered but no
    // queue element exists.
    if (!_message->buffers_.empty())
      send(_message);
    // LCOV_EXCL_STOP

    compact_buffer_if_needed();
    do_read();
  }
} // namespace throttr