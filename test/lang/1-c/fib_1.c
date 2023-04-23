#include "fib.h"

#include <stdio.h>

const unsigned short version = 1;

static long fibalgo(long value) {
	if (value < 0)
		return 0;
	else if (value < 2)
		return value;
	else
		return fibalgo(value - 1) + fibalgo(value - 2);
}

long fib(long value) {
	return fibalgo(value);
}

void printfib(long value) {
	printf("[C Fibonacci Library v%d] fib(%ld) = %ld\n", version, value, fibalgo(value));
	fflush(stdout);
}