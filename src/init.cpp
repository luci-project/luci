#include "init.hpp"

#include <cstdarg>
#include <capstone/capstone.h>

#include "ostream.hpp"
#include "bufstream.hpp"
#include "alloc.hpp"

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
