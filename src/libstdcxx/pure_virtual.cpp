#include "utils/stream/output.hpp"
#include "libc/unistd.hpp"

extern "C" [[noreturn]] void __cxa_pure_virtual() {
	cerr <<  "Pure virtual function was called -- this if obviously not valid!" << endl;
	abort();
}
