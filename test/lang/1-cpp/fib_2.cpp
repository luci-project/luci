#include "fib.h"

#include <iostream>

const unsigned short version = 2;

static long fibalgo(long value) {
	long l = 0;
	long p = 1;
	long n = value;
	for (long i = 1; i < value; i++) {
		n = l + p;
		l = p;
		p = n;
	}
	return n;
}

long fib(long value) {
	return fibalgo(value);
}

void printfib(long value) {
	std::cout << "[C++ Fibonacci Library v" << version << "] fib(" << value << ") = " << fib(value) << std::endl << std::flush;
}