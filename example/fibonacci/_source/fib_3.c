#include "fib.h"

const unsigned short version = 3;

unsigned long fib(unsigned long value) {
	unsigned long l = 0;
	unsigned long p = 1;
	unsigned long n = value;
	for (unsigned long i = 1; i < value; i++) {
		n = l + p;
		l = p;
		p = n;
	}
	return n;
}

int print_library_info(FILE *stream) {
	return fprintf(stream, "[using Fibonacci library v%u: O(n)]\n", version);
}

