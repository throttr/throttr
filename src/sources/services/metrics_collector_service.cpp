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

#include <throttr/services/metrics_collector_service.hpp>

#include <throttr/state.hpp>
#include <throttr/utils.hpp>

#include <boost/asio/bind_executor.hpp>

#include <throttr/connection.hpp>

namespace throttr
{
#ifdef ENABLED_FEATURE_METRICS
  void metrics_collector_service::schedule_timer(const std::shared_ptr<state> &state)
  {
#ifndef NDEBUG
    fmt::
      println("[{}] [{:%Y-%m-%d %H:%M:%S}] METRICS SNAPSHOT SCHEDULED", to_string(state->id_), std::chrono::system_clock::now());
#endif // NDEBUG

    state->metrics_timer_.expires_after(std::chrono::minutes(1));
    state->metrics_timer_.async_wait(boost::asio::bind_executor(
      state->strand_,
      [_self = shared_from_this(), _state = state->shared_from_this()](const boost::system::error_code &ec)
      {
        if (!ec)
        {
          run(_state);
          _self->schedule_timer(_state); // reschedule
        }
      }));
  }

  void metrics_collector_service::run(const std::shared_ptr<state> &state)
  {
#ifndef NDEBUG
    fmt::println("[{}] [{:%Y-%m-%d %H:%M:%S}] METRICS SNAPSHOT STARTED", to_string(state->id_), std::chrono::system_clock::now());
#endif // NDEBUG
    for (auto &_index = state->storage_.get<tag_by_key>(); auto &_entry : _index)
    {
      if (_entry.expired_)
        continue;

      auto &[reads_, writes_, reads_accumulator_, writes_accumulator_, reads_per_minute_, writes_per_minute_] = *_entry.metrics_;
      const auto reads = reads_.exchange(0, std::memory_order_relaxed);
      const auto writes = writes_.exchange(0, std::memory_order_relaxed);

      reads_per_minute_.store(reads, std::memory_order_relaxed);
      writes_per_minute_.store(writes, std::memory_order_relaxed);

      reads_accumulator_.fetch_add(reads, std::memory_order_relaxed);
      writes_accumulator_.fetch_add(writes, std::memory_order_relaxed);
    }

    auto _compute_metrics = [](const auto &connections, auto &mutex)
    {
      std::scoped_lock _lock(mutex);
      for (const auto &_conn : connections)
      {
        for (auto &_m : _conn.second->metrics_->commands_)
          _m.compute();
      }
    };

    _compute_metrics(state->tcp_connections_, state->tcp_connections_mutex_);
    _compute_metrics(state->unix_connections_, state->unix_connections_mutex_);
    _compute_metrics(state->agent_unix_connections_, state->agent_unix_connections_mutex_);
    _compute_metrics(state->agent_tcp_connections_, state->agent_tcp_connections_mutex_);

    state->metrics_collector_->compute_all();

#ifndef NDEBUG
    fmt::
      println("[{}] [{:%Y-%m-%d %H:%M:%S}] METRICS SNAPSHOT COMPLETED", to_string(state->id_), std::chrono::system_clock::now());
#endif // NDEBUG
  }

  void metrics_collector_service::compute_all()
  {
    for (auto &m : commands_)
      m.compute();
  }
#endif // ENABLED_FEATURE_METRICS
} // namespace throttr
