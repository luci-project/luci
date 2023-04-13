#include <iostream>
#include <stdexcept>

#include "foo.hpp"
#include "bar.hpp"

int foo(int i) {
	try {
		std::cout << " foo v" << VERSION << ": i = " << i << std::endl;
		return bar(i * 23) + 10 * VERSION;
	} catch (std::invalid_argument& e) {
		std::cout << " foo: failed due to invalid argument (" << i << ") @ v" << VERSION << std::endl;
		return -10 * VERSION;
	}
	return -1 * VERSION;
}
