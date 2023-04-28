// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include "loader.hpp"

namespace GLIBC {
void* start_entry(Loader & loader);
}  // namespace GLIBC
