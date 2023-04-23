#include "fib.h"

#include <iostream>

const unsigned short version = 0;

static long fibalgo(long value) {
	if (value < 2)
		return value;
	else
		return fibalgo(value - 1) + fibalgo(value - 2);
}

long fib(long value) {
	return fibalgo(value);
}

void printfib(long value) {
	std::cout << "[C++ Fibonacci Library v" << version << "] fib(" << value << ") = " << fib(value) << std::endl << std::flush;
}