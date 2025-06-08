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

#pragma once

#include <boost/asio/bind_allocator.hpp>
#include <boost/asio/write.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <throttr/services/commands_service.hpp>
#include <throttr/services/messages_service.hpp>
#include <throttr/services/metrics_collector_service.hpp>
#include <throttr/state.hpp>
#include <throttr/utils.hpp>

namespace throttr
{
  template<typename Transport>
  connection<Transport>::connection(Transport socket, const std::shared_ptr<state> &state) :
      id_(state->id_generator_()), socket_(std::move(socket)), state_(state)
  {

    // LCOV_EXCL_START
    if (socket_.is_open())
    {
      if constexpr (std::is_same_v<Transport, tcp_socket>)
      {
        kind_ = connection_kind::tcp_socket;
        const boost::asio::ip::tcp::no_delay no_delay_option(true);
        socket_.set_option(no_delay_option);
        ip_ = socket_.remote_endpoint().address().to_string();
        port_ = socket_.remote_endpoint().port();
      }
      else
      {
        kind_ = connection_kind::unix_socket;
      }
      connected_at_ =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
    // LCOV_EXCL_STOP
  }

  template<typename Transport> connection<Transport>::~connection()
  {
    // LCOV_EXCL_START
#ifndef NDEBUG
    switch (kind_)
    {
      case connection_kind::tcp_socket:
        fmt::println(
          "{:%Y-%m-%d %H:%M:%S} TCP SESSION CLOSED session_id={} META ip={} port={}",
          std::chrono::system_clock::now(),
          to_string(id_),
          ip_,
          port_);
        break;
      case connection_kind::unix_socket:
        fmt::println("{:%Y-%m-%d %H:%M:%S} UNIX SESSION CLOSED session_id={}", std::chrono::system_clock::now(), to_string(id_));
        break;
    }
#endif
    // LCOV_EXCL_STOP
    state_->leave(this);
  }

  template<typename Transport> void connection<Transport>::start()
  {
    // LCOV_EXCL_START
#ifndef NDEBUG
    switch (kind_)
    {
      case connection_kind::tcp_socket:
        fmt::println(
          "{:%Y-%m-%d %H:%M:%S} TCP SESSION ESTABLISHED session_id={} META ip={} port={}",
          std::chrono::system_clock::now(),
          to_string(id_),
          ip_,
          port_);
        break;
      case connection_kind::unix_socket:
        fmt::println(
          "{:%Y-%m-%d %H:%M:%S} UNIX SESSION ESTABLISHED session_id={}", std::chrono::system_clock::now(), to_string(id_));
        break;
    }
#endif
    // LCOV_EXCL_STOP

    state_->join(this);

    do_read();
  }

  template<typename Transport> void connection<Transport>::compact_buffer_if_needed()
  {
    if (buffer_start_ == buffer_end_) // LCOV_EXCL_LINE note: Partially tested.
    {
      buffer_start_ = 0;
      buffer_end_ = 0;
    }
    else if (buffer_start_ > max_length_ / 2) // LCOV_EXCL_LINE note: Partially tested.
    {
      compact_buffer();
    }
  }

  template<typename Transport> void connection<Transport>::compact_buffer()
  {
    if (buffer_start_ == buffer_end_) // LCOV_EXCL_LINE note: Partially tested.
      return;                         // LCOV_EXCL_LINE note: Partially tested.
    std::memmove(buffer_.data(), buffer_.data() + buffer_start_, buffer_end_ - buffer_start_);
    buffer_end_ -= buffer_start_;
    buffer_start_ = 0;
  }

  template<typename Transport> void connection<Transport>::send(std::shared_ptr<message> batch)
  {
    auto self = this->shared_from_this();
    boost::asio::post(socket_.get_executor(), [self, _batch = std::move(batch)]() mutable { self->on_send(_batch); });
  }

  template<typename Transport> void connection<Transport>::on_read(const boost::system::error_code &error, std::size_t length)
  {
    if (!error) // LCOV_EXCL_LINE note: Partially tested.
    {
#ifdef ENABLED_FEATURE_METRICS
      metrics_->network_.read_bytes_.mark(length);
#endif

      buffer_end_ += length;
      try_process_next();
      // LCOV_EXCL_START
    }
    else
    {
      close();
    }
    // LCOV_EXCL_STOP
  }

  template<typename Transport> void connection<Transport>::try_process_next()
  {
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
      switch (kind_)
      {
        case connection_kind::tcp_socket:
          fmt::println(
            "{:%Y-%m-%d %H:%M:%S} TCP SESSION READ session_id={} META ip={} port={} buffer={}",
            std::chrono::system_clock::now(),
            to_string(id_),
            ip_,
            port_,
            span_to_hex(_view));
          break;
        case connection_kind::unix_socket:
          fmt::println(
            "{:%Y-%m-%d %H:%M:%S} UNIX SESSION READ session_id={} META buffer={}",
            std::chrono::system_clock::now(),
            to_string(id_),
            span_to_hex(_view));
          break;
      }
#endif
      // LCOV_EXCL_STOP

      const auto _type = static_cast<request_types>(std::to_integer<uint8_t>(_view[0]));
#ifdef ENABLED_FEATURE_METRICS
      state_->metrics_collector_->commands_[static_cast<std::size_t>(_type)].mark();
      metrics_->commands_[static_cast<std::size_t>(_type)].mark();
#endif
      state_->commands_->commands_[static_cast<std::size_t>(
        _type)](state_, _type, _view, message_->buffers_, message_->write_buffer_, this->id_);
      // LCOV_EXCL_STOP
    }

    // LCOV_EXCL_START Note: Partially tested.
    // The not tested case is when in-while break condition is triggered but no
    // queue element exists.
    if (!message_->buffers_.empty())
      send(message_);
    // LCOV_EXCL_STOP

    compact_buffer_if_needed();
    do_read();
  }

  template<typename Transport> std::size_t connection<Transport>::get_message_size(std::span<const std::byte> buffer) const
  {
    if (buffer.empty())
      return 0;

    const auto *_buffer = buffer.data();

    const auto _type = static_cast<request_types>(std::to_integer<uint8_t>(_buffer[0]));
    return state_->messages_->message_types_[static_cast<std::size_t>(_type)](buffer);
  }

  template<typename Transport> void connection<Transport>::do_read()
  {
    // LCOV_EXCL_START
    if (buffer_end_ == max_length_)
    {
      compact_buffer();
    }
    // LCOV_EXCL_STOP

    auto _self = this->shared_from_this();
    socket_.async_read_some(
      boost::asio::buffer(buffer_.data() + buffer_end_, max_length_ - buffer_end_),
      boost::asio::bind_allocator(
        connection_handler_allocator<int>(handler_memory_),
        [_self](const boost::system::error_code &ec, const std::size_t length) { _self->on_read(ec, length); }));
  }

  template<typename Transport> void connection<Transport>::on_write(const boost::system::error_code &error, std::size_t length)
  {
    boost::ignore_unused(length);

#ifdef ENABLED_FEATURE_METRICS
    metrics_->network_.write_bytes_.mark(length);
#endif

    // LCOV_EXCL_START
    if (error)
    {
      close();
      return;
    }

    const std::shared_ptr<message> _message = pending_writes_.front();
    pending_writes_.pop_front();
    if (_message->recyclable_)
    {
      _message->write_buffer_.clear();
      _message->buffers_.clear();
    }

    // LCOV_EXCL_START
    if (!pending_writes_.empty())
    {
      write_next();
    }
    // LCOV_EXCL_STOP
  }

  template<typename Transport> void connection<Transport>::close()
  {
    boost::system::error_code ec;
    if constexpr (std::is_same_v<Transport, tcp_socket>)
    {
      socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    }
    socket_.close(ec);
  }

  template<typename Transport> void connection<Transport>::on_send(const std::shared_ptr<message> &batch)
  {
    pending_writes_.push_back(batch);

    // LCOV_EXCL_START Note: This is partially tested.
    if (pending_writes_.size() > 1)
      return;
    // LCOV_EXCL_STOP

    write_next();
  }

  template<typename Transport> void connection<Transport>::write_next()

  {
    // LCOV_EXCL_START
#ifndef NDEBUG
    switch (kind_)
    {
      case connection_kind::tcp_socket:
        fmt::println(
          "{:%Y-%m-%d %H:%M:%S} TCP SESSION WRITE session_id={} META ip={} port={} buffer={}",
          std::chrono::system_clock::now(),
          to_string(id_),
          ip_,
          port_,
          buffers_to_hex(pending_writes_.front()->buffers_));
        break;
      case connection_kind::unix_socket:
        fmt::println(
          "{:%Y-%m-%d %H:%M:%S} UNIX SESSION WRITE session_id={} META buffer={}",
          std::chrono::system_clock::now(),
          to_string(id_),
          buffers_to_hex(pending_writes_.front()->buffers_));
    }
#endif
    // LCOV_EXCL_STOP

    boost::asio::async_write(
      socket_,
      pending_writes_.front()->buffers_,
      boost::asio::bind_allocator(
        connection_handler_allocator<int>(handler_memory_),
        [_self = this->shared_from_this()](const boost::system::error_code &ec, const std::size_t length)
        { _self->on_write(ec, length); }));
  }
} // namespace throttr