#pragma once

#include "loader.hpp"

void gdb_initialize(const Loader & loader);
void gdb_notify();
static inline void gdb_breakpoint() {
	asm volatile("int3" ::: "memory");
}
