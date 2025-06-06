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

  boost::program_options::options_description _options("Options");

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

  _options.add_options()(
    "socket",
    boost::program_options::value<std::string>()->default_value("throttr.sock"),

    "Assigned socket path.")("port", boost::program_options::value<short>()->default_value(9000), "Assigned port.")(
    "threads", boost::program_options::value<int>()->default_value(default_threads), "Assigned threads.");

  boost::program_options::variables_map _vm;
  store(parse_command_line(argc, argv, _options), _vm);

  throttr::program_options _program_options{
    .socket_ = _vm["socket"].as<std::string>(),
    .port_ = _vm["port"].as<short>(),
  };

  const auto _threads = _vm["threads"].as<int>();

  const auto _app = std::make_shared<throttr::app>(_program_options, _threads);

  return _app->serve();
}