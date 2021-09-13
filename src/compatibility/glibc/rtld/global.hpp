#pragma once

#include "loader.hpp"

namespace GLIBC {
namespace RTLD {
void init_globals(const Loader & loader);
void stack_end(void * ptr);
}  // namespace RTLD
}  // nammespace GLIBC
