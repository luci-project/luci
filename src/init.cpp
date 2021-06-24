#include "init.hpp"

#include <dlh/alloc.hpp>
#include <dlh/stdarg.hpp>
#include <dlh/stream/output.hpp>
#include <dlh/stream/buffer.hpp>

#include <capstone/capstone.h>


int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
	return BufferStream(str, size).format(format, ap);
}

bool init() {
	// Capstone (used by Bean) without libc
	cs_opt_mem setup = {
		.malloc = malloc,
		.calloc = calloc,
		.realloc = realloc,
		.free = free,
		.vsnprintf = vsnprintf
	};
	if (cs_option(0, CS_OPT_MEM, reinterpret_cast<size_t>(&setup)) != 0) {
		cerr << "Capstone failed" << endl;
		return false;
	}

	return true;
}
