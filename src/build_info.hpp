// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <dlh/stream/buffer.hpp>

namespace BuildInfo {

/*! \brief Print build information in stream */
void print(BufferStream & stream, bool verbose);

/*! \brief Print build information in log */
void log();

}  // namespace BuildInfo
