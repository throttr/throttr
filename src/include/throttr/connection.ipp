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
#include <boost/beast/core/bind_handler.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <throttr/services/commands_service.hpp>
#include <throttr/services/messages_service.hpp>
#include <throttr/services/metrics_collector_service.hpp>

#include <throttr/state.hpp>
#include <throttr/utils.hpp>

#include <throttr/messages_pool.hpp>

namespace throttr
{
  template<typename Transport>
  connection<Transport>::connection(Transport socket, const std::shared_ptr<state> &state, const connection_type type) :
      id_(state->id_generator_()), type_(type), socket_(std::move(socket)), state_(state)
  {

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
  }

  template<typename Transport> connection<Transport>::~connection()
  {
#ifndef NDEBUG
    switch (kind_)
    {
      case connection_kind::tcp_socket:
        fmt::println(
          "[{}] [{:%Y-%m-%d %H:%M:%S}] TCP SESSION CLOSED session_id={} META ip={} port={}",
          to_string(state_->id_),
          std::chrono::system_clock::now(),
          to_string(id_),
          ip_,
          port_);
        break;
      case connection_kind::unix_socket:
        fmt::println(
          "[{}] [{:%Y-%m-%d %H:%M:%S}] UNIX SESSION CLOSED session_id={}",
          to_string(state_->id_),
          std::chrono::system_clock::now(),
          to_string(id_));
        break;
    }
#endif
    state_->leave(this);
  }

  template<typename Transport> void connection<Transport>::start()
  {
#ifndef NDEBUG
    if (type_ == connection_type::client)
    {
      switch (kind_)
      {
        case connection_kind::tcp_socket:
          fmt::println(
            "[{}] [{:%Y-%m-%d %H:%M:%S}] TCP SESSION ESTABLISHED session_id={} META ip={} port={}",
            to_string(state_->id_),
            std::chrono::system_clock::now(),
            to_string(id_),
            ip_,
            port_);
          break;
        case connection_kind::unix_socket:
          fmt::println(
            "[{}] [{:%Y-%m-%d %H:%M:%S}] UNIX SESSION ESTABLISHED session_id={}",
            to_string(state_->id_),
            std::chrono::system_clock::now(),
            to_string(id_));
          break;
      }
    }
    else
    {
      switch (kind_)
      {
        case connection_kind::tcp_socket:
          fmt::println(
            "[{}] [{:%Y-%m-%d %H:%M:%S}] TCP AGENT ESTABLISHED META ip={} port={}",
            to_string(state_->id_),
            std::chrono::system_clock::now(),
            to_string(id_),
            ip_,
            port_);
          break;
        case connection_kind::unix_socket:
          fmt::println(
            "[{}] [{:%Y-%m-%d %H:%M:%S}] UNIX AGENT ESTABLISHED",
            to_string(state_->id_),
            std::chrono::system_clock::now(),
            to_string(id_));
          break;
      }
    }
#endif

    state_->join(this);

    do_read();
  }

  template<typename Transport> void connection<Transport>::compact_buffer_if_needed()
  {
    if (buffer_start_ == buffer_end_)
    {
      buffer_start_ = 0;
      buffer_end_ = 0;
    }
    else if (buffer_start_ > max_length_ / 2)
    {
      compact_buffer();
    }
  }

  template<typename Transport> void connection<Transport>::compact_buffer()
  {
    if (buffer_start_ == buffer_end_)
      return;
    std::memmove(buffer_.data(), buffer_.data() + buffer_start_, buffer_end_ - buffer_start_);
    buffer_end_ -= buffer_start_;
    buffer_start_ = 0;
  }

  template<typename Transport> void connection<Transport>::send(const std::shared_ptr<message> &batch)
  {
    boost::asio::
      post(socket_.get_executor(), boost::beast::bind_front_handler(&connection::on_send, this->shared_from_this(), batch));
  }

  template<typename Transport> void connection<Transport>::on_read(const boost::system::error_code &error, std::size_t length)
  {
    if (!error)
    {
#ifdef ENABLED_FEATURE_METRICS
      metrics_->network_.read_bytes_.mark(length);
#endif

      buffer_end_ += length;
      try_process_next();
    }
    else
    {
      close();
    }
  }

  template<typename Transport> void connection<Transport>::try_process_next()
  {
    messages_pool::recycle();
    messages_pool::fit();

    const auto _message = messages_pool::take_one();

    while (true)
    {
      const std::span<const std::byte> _span(buffer_.data() + buffer_start_, buffer_end_ - buffer_start_);
      const std::size_t _msg_size = get_message_size(_span);
      if (_msg_size == 0 || _span.size() < _msg_size)
        break;

      const std::span<const std::byte> _view(buffer_.data() + buffer_start_, _msg_size);
      buffer_start_ += _msg_size;

#ifndef NDEBUG
      switch (kind_)
      {
        case connection_kind::tcp_socket:
          fmt::println(
            "[{}] [{:%Y-%m-%d %H:%M:%S}] TCP SESSION READ session_id={} META ip={} port={} buffer={}",
            to_string(state_->id_),
            std::chrono::system_clock::now(),
            to_string(id_),
            ip_,
            port_,
            span_to_hex(_view));
          break;
        case connection_kind::unix_socket:
          fmt::println(
            "[{}] [{:%Y-%m-%d %H:%M:%S}] UNIX SESSION READ session_id={} META buffer={}",
            to_string(state_->id_),
            std::chrono::system_clock::now(),
            to_string(id_),
            span_to_hex(_view));
          break;
      }
#endif

      const auto _type = static_cast<request_types>(std::to_integer<uint8_t>(_view[0]));
#ifdef ENABLED_FEATURE_METRICS
      state_->metrics_collector_->commands_[static_cast<std::size_t>(_type)].mark();
      metrics_->commands_[static_cast<std::size_t>(_type)].mark();
#endif

      state_->commands_->commands_[static_cast<std::size_t>(
        _type)](state_, _type, _view, _message->buffers_, _message->write_buffer_, this->id_);
    }

    if (_message->buffers_.size() > 0)
    {
      send(_message);
    }
    else
    {
      _message->in_use_ = false;
    }

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
    if (buffer_end_ == max_length_)
    {
      compact_buffer();
    }

    auto _self = this->shared_from_this();
    socket_.async_read_some(
      boost::asio::buffer(buffer_.data() + buffer_end_, max_length_ - buffer_end_),
      boost::asio::bind_allocator(
        custom_allocator<int>(read_memory_),
        [_self](const boost::system::error_code &ec, const std::size_t length) { _self->on_read(ec, length); }));
  }

  template<typename Transport> void connection<Transport>::on_write(const boost::system::error_code &error, std::size_t length)
  {
    boost::ignore_unused(length);

#ifdef ENABLED_FEATURE_METRICS
    metrics_->network_.write_bytes_.mark(length);
#endif

    if (error)
    {
      close();
      return;
    }

    if (pending_writes_.empty())
      return;

    const std::shared_ptr<message> _message = pending_writes_.front();
    pending_writes_.pop_front();

    if (_message->recyclable_)
    {
      _message->in_use_ = false;
    }

    if (!pending_writes_.empty())
    {
      write_next();
    }
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

    if (pending_writes_.size() > 1)
      return;

    write_next();
  }

  template<typename Transport> void connection<Transport>::write_next()
  {
    std::scoped_lock _write_guard(mutex_);

    if (pending_writes_.empty())
      return;

#ifndef NDEBUG
    switch (kind_)
    {
      case connection_kind::tcp_socket:
        fmt::println(
          "[{}] [{:%Y-%m-%d %H:%M:%S}] TCP SESSION WRITE session_id={} META ip={} port={} buffer={}",
          to_string(state_->id_),
          std::chrono::system_clock::now(),
          to_string(id_),
          ip_,
          port_,
          buffers_to_hex(pending_writes_.front()->buffers_));
        break;
      case connection_kind::unix_socket:
        fmt::println(
          "[{}] [{:%Y-%m-%d %H:%M:%S}] UNIX SESSION WRITE session_id={} META buffer={}",
          to_string(state_->id_),
          std::chrono::system_clock::now(),
          to_string(id_),
          buffers_to_hex(pending_writes_.front()->buffers_));
    }
#endif

    boost::asio::async_write(
      socket_,
      pending_writes_.front()->buffers_,
      boost::asio::bind_allocator(
        custom_allocator<int>(write_memory_),
        [_self = this->shared_from_this()](const boost::system::error_code &ec, const std::size_t length)
        { _self->on_write(ec, length); }));
  }
} // namespace throttr