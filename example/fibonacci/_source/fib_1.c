#include "fib.h"

const unsigned short version = 1;

unsigned long fib(unsigned long value) {
	return value > 1 ? fib(value - 1) + fib(value - 2) : value;
}

int print_library_info(FILE *stream) {
	return fprintf(stream, "[using Fibonacci library v%u: O(2^n))]\n", version);
}
