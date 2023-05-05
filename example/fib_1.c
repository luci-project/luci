#include <stdio.h>

#include "fib.h"

const unsigned short version = 1;

unsigned long fib(unsigned long value) {
	if (value < 2)
		return value;
	else
		return fib(value - 1) + fib(value - 2);
}

void dump_info(void) {
	fprintf(stderr, "[using Fibonacci library v%u: O(2^n))]\n", version);
}
