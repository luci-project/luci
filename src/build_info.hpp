#pragma once

#include <dlh/stream/buffer.hpp>

namespace BuildInfo {

/*! \brief Print build information in stream */
void print(BufferStream & stream, bool verbose);

/*! \brief Print build information in log */
void log();

}  // namespace BuildInfo
