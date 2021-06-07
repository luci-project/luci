#include <dlh/stream/output.hpp>
#include <dlh/unistd.hpp>

extern "C" [[noreturn]] void __cxa_pure_virtual() {
	cerr <<  "Pure virtual function was called -- this if obviously not valid!" << endl;
	abort();
}
