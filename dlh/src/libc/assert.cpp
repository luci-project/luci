#include <dlh/assert.hpp>

#include "libc/unistd.hpp"
#include "utils/stream/output.hpp"

[[noreturn]] void __assert_fail(const char * exp, const char * file, int line, const char * func) {
	// Do not use assertions in OutputStream!
	cerr << "Assertion '" << exp << "' failed (" << func << " @ " << file
	     << ":" << dec << line << ")" << endl;

	abort();
}
