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
#include <throttr/protocol_wrapper.hpp>
#include <throttr/state.hpp>

namespace throttr
{
  std::size_t connection::get_message_size(const std::span<const std::byte> buffer) const
  {
    if (buffer.empty())
      return 0;

    const auto *_buffer = buffer.data();

    const auto _type = static_cast<request_types>(std::to_integer<uint8_t>(_buffer[0]));
    return state_->messages_->message_types_[static_cast<std::size_t>(_type)](buffer);
  }
} // namespace throttr