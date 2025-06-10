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

#include <fstream>
#include <stdexcept>
#include <throttr/entry_wrapper.hpp>
#include <throttr/protocol_wrapper.hpp>
#include <throttr/storage.hpp>
#include <throttr/storage_serializer.hpp>

namespace throttr
{
  namespace
  {
    constexpr const char *MAGIC_ = "THRT";
    constexpr uint8_t VERSION_ = 1;

    inline uint8_t get_value_size_code()
    {
      if constexpr (sizeof(value_type) == 1)
        return 1;
      else if constexpr (sizeof(value_type) == 2)
        return 2;
      else if constexpr (sizeof(value_type) == 4)
        return 4;
      else if constexpr (sizeof(value_type) == 8)
        return 8;
      else
        static_assert(sizeof(value_type) == 1 || sizeof(value_type) == 2 || sizeof(value_type) == 4 || sizeof(value_type) == 8);
    }

    template<typename T> void write_raw(std::ofstream &out, const T &value)
    {
      out.write(reinterpret_cast<const char *>(&value), sizeof(T));
    }

    template<typename T> void read_raw(std::ifstream &in, T &value)
    {
      in.read(reinterpret_cast<char *>(&value), sizeof(T));
    }

    uint64_t read_metric(std::ifstream &_in)
    {
      uint64_t _value;
      read_raw(_in, _value);
      return _value;
    }

    void write_bytes(std::ofstream &out, const std::vector<std::byte> &buffer)
    {
      out.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
    }

    void read_bytes(std::ifstream &in, std::vector<std::byte> &buffer, std::size_t size)
    {
      buffer.resize(size);
      in.read(reinterpret_cast<char *>(buffer.data()), size);
    }
  } // namespace

  void dump_to_file(const storage_type &storage, const std::string &filename)
  {
    std::ofstream _out(filename, std::ios::binary);
    if (!_out)
      throw std::runtime_error("Unable to open dump file for writing");

    _out.write(MAGIC_, 4);
    write_raw(_out, VERSION_);
    write_raw(_out, get_value_size_code());

    uint32_t _count = 0;
    for (const auto &_entry : storage)
    {
      if (!_entry.expired_)
        ++_count;
    }
    write_raw(_out, _count);

    for (const auto &_e : storage)
    {
      if (_e.expired_)
        continue;

      const auto &_key = _e.key_;
      uint16_t _key_size = static_cast<uint16_t>(_key.size());
      write_raw(_out, _key_size);
      write_bytes(_out, _key);

      write_raw(_out, _e.entry_.expires_at_.load(std::memory_order_relaxed));
      write_raw(_out, static_cast<uint8_t>(_e.entry_.type_));
      write_raw(_out, static_cast<uint8_t>(_e.entry_.ttl_type_));

#ifdef ENABLED_FEATURE_METRICS
      write_raw(_out, _e.metrics_->reads_.load());
      write_raw(_out, _e.metrics_->writes_.load());
      write_raw(_out, _e.metrics_->reads_accumulator_.load());
      write_raw(_out, _e.metrics_->writes_accumulator_.load());
      write_raw(_out, _e.metrics_->reads_per_minute_.load());
      write_raw(_out, _e.metrics_->writes_per_minute_.load());
#endif

      if (_e.entry_.type_ == entry_types::counter)
      {
        value_type _counter = static_cast<value_type>(_e.entry_.counter_.load());
        write_raw(_out, _counter);
      }
      else if (_e.entry_.type_ == entry_types::raw)
      {
        const auto _buffer_ptr = _e.entry_.buffer_.load();
        value_type _buffer_size = static_cast<value_type>(_buffer_ptr->size());
        write_raw(_out, _buffer_size);
        _out.write(reinterpret_cast<const char *>(_buffer_ptr->data()), _buffer_size);
      }
    }
  }

  void restore_from_file(storage_type &storage, const std::string &filename)
  {
    std::ifstream _in(filename, std::ios::binary);
    if (!_in)
      throw std::runtime_error("Unable to open dump file for reading");

    char _magic[4];
    _in.read(_magic, 4);
    if (std::string_view(_magic, 4) != MAGIC_)
      throw std::runtime_error("Invalid dump file format");

    uint8_t _version = 0;
    read_raw(_in, _version);
    if (_version != VERSION_)
      throw std::runtime_error("Unsupported dump file version");

    uint8_t _file_value_size = 0;
    read_raw(_in, _file_value_size);
    if (_file_value_size != get_value_size_code())
      throw std::runtime_error("Mismatched value_size in dump file");

    uint32_t _count = 0;
    read_raw(_in, _count);

    for (uint32_t _i = 0; _i < _count; ++_i)
    {
      uint16_t _key_size;
      read_raw(_in, _key_size);

      std::vector<std::byte> _key;
      read_bytes(_in, _key, _key_size);

      uint64_t _expires_at;
      read_raw(_in, _expires_at);

      uint8_t _type_raw;
      read_raw(_in, _type_raw);
      entry_types _type = static_cast<entry_types>(_type_raw);

      uint8_t _ttl_raw;
      read_raw(_in, _ttl_raw);
      ttl_types _ttl_type = static_cast<ttl_types>(_ttl_raw);

      entry _new_entry;
      _new_entry.type_ = _type;
      _new_entry.expires_at_.store(_expires_at);
      _new_entry.ttl_type_ = _ttl_type;

#ifdef ENABLED_FEATURE_METRICS
      auto _metrics = std::make_shared<entry_metrics>();
      _metrics->reads_.store(read_metric(_in), std::memory_order_relaxed);
      _metrics->writes_.store(read_metric(_in), std::memory_order_relaxed);
      _metrics->reads_accumulator_.store(read_metric(_in), std::memory_order_relaxed);
      _metrics->writes_accumulator_.store(read_metric(_in), std::memory_order_relaxed);
      _metrics->reads_per_minute_.store(read_metric(_in), std::memory_order_relaxed);
      _metrics->writes_per_minute_.store(read_metric(_in), std::memory_order_relaxed);
#endif

      if (_type == entry_types::counter)
      {
        value_type _counter;
        read_raw(_in, _counter);
        _new_entry.counter_.store(_counter);
      }
      else if (_type == entry_types::raw)
      {
        value_type _buffer_size;
        read_raw(_in, _buffer_size);
        std::vector<std::byte> _buffer;
        read_bytes(_in, _buffer, _buffer_size);
        _new_entry.buffer_.store(std::make_shared<std::vector<std::byte>>(std::move(_buffer)));
      }

#ifdef ENABLED_FEATURE_METRICS
      auto _entry_wrapper = entry_wrapper{std::move(_key), std::move(_new_entry)};
      _entry_wrapper.metrics_ = _metrics;
      storage.insert(_entry_wrapper);
#else
      storage.insert(entry_wrapper{std::move(_key), std::move(_new_entry)});
#endif
    }
  }
} // namespace throttr