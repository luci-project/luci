#include "fib.h"

const unsigned short version = 2;

unsigned long fib(unsigned long value) {
	unsigned long f[value + 2];
	for (unsigned long n = 0; n <= value; n++)
		f[n] = n > 1 ? f[n - 1] + f[n - 2] : n;
	return f[value];
}

int print_library_info(FILE *stream) {
	return fprintf(stream, "[using Fibonacci library v%u: O(n))]\n", version);
}
