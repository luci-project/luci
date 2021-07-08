#pragma once

#include "loader.hpp"

namespace GDB {
void init(const Loader & loader);
void notify();
static inline void breakpoint() {
	asm volatile("int3" ::: "memory");
}
}  // namespace GDB
