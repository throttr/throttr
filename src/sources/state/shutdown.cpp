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

#include <throttr/program_parameters.hpp>

#include <throttr/state.hpp>
#include <throttr/storage.hpp>
#include <throttr/storage_serializer.hpp>

namespace throttr
{
  void state::prepare_for_shutdown(const program_parameters &parameters)
  {
    if (parameters.persistent_) // LCOV_EXCL_LINE
    {
      dump_to_file(storage_, parameters.dump_);
    }
  }
  void state::prepare_for_startup(const program_parameters &parameters)
  {
    if (parameters.persistent_) // LCOV_EXCL_LINE
    {
      restore_from_file(storage_, parameters.dump_);
    }
  }
} // namespace throttr