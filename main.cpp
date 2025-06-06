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

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <throttr/app.hpp>

int
main(const int argc, const char *argv[])
{
  using namespace boost::program_options;
  using namespace throttr;
  options_description _options("Options");

  int default_threads = 1;
  if (const char *env_threads = std::getenv("THREADS"))
  {
    try
    {
      default_threads = std::max(1, std::stoi(env_threads));
    }
    catch (...)
    {
      default_threads = 1;
    }
  }

  auto _push_option = _options.add_options();

  _push_option("socket", value<std::string>()->default_value("throttr.sock"));
  _push_option("port", value<short>()->default_value(9000));
  _push_option("threads", value<int>()->default_value(default_threads));
  _push_option("as_master", value<bool>()->default_value(true));
  _push_option("master_host", value<std::string>()->default_value("127.0.0.1"));
  _push_option("master_port", value<short>()->default_value(9000));

  variables_map _vm;
  store(parse_command_line(argc, argv, _options), _vm);

  program_parameters _program_options{
    .socket_ = _vm["socket"].as<std::string>(),
    .port_ = _vm["port"].as<short>(),
    .threads_ = _vm["threads"].as<int>(),
    .as_master_ = _vm["as_master"].as<bool>(),
    .master_host_ = _vm["master_host"].as<std::string>(),
    .master_port_ = _vm["master_port"].as<short>(),
  };

  const auto _app = std::make_shared<app>(_program_options);

  return _app->serve();
}