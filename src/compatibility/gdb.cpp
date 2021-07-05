#include "gdb.hpp"

#include <dlh/assert.hpp>

extern "C" void gdb_initialize(ObjectIdentity *main) {
	assert(false);
}

extern "C" void gdb_notify() {
	assert(false);
}
