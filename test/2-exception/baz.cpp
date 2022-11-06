#include <iostream>
#include <stdexcept>

#include "baz.hpp"

#define str(s) xstr(s)
#define xstr(s) #s

void unused(int i) {
	throw std::range_error("Unused cannot handle any values");
}

int baz(int i) {
	if (i < 0) {
		std::cout << "   baz v" << VERSION << ": i = " << i << " (invalid argument)" << std::endl;
		throw std::invalid_argument("Baz v" str(VERSION) " doesn't allow negative values");
	}
	else if (i == 0) {
		std::cout << "   baz v" << VERSION << ": i = " << i << " (runtime error)" << std::endl;
		throw std::runtime_error("Baz v" str(VERSION) " cannot handle zero values");
	}
	#if VERSION > 1
	else if (i > 42) {
		std::cout << "   baz v" << VERSION << ": i = " << i << " (range error)" << std::endl;
		throw std::range_error("Baz v" str(VERSION) " cannot handle big values");
	}
	#endif

	std::cout << "   baz v" << VERSION << ": i = " << i << std::endl;
	return i * 1337 + VERSION;

}
