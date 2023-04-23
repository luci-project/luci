#include "fib.h"

#include <stdio.h>

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
	printf("[C Fibonacci Library v%d] fib(%ld) = %ld\n", version, value, fibalgo(value));
	fflush(stdout);
}