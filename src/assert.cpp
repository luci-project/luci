#include "assert.hpp"

#include "ostream.hpp"
#include "stdlib.hpp"

[[noreturn]] void __assert_fail(const char * exp, const char * file, int line, const char * func) {
	// Do not use assertions in OutputStream!
	cerr << "Assertion '" << exp << "' failed (" << func << " @ " << file
	     << ":" << dec << line << ")" << endl;

	abort();
}
