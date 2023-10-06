#include "fib.h"

const unsigned short version = 4;

static void multiply(unsigned long f[4], const unsigned long m[4]) {
	unsigned long a = f[0] * m[0] + f[1] * m[2];
	unsigned long b = f[0] * m[1] + f[1] * m[3];
	unsigned long c = f[2] * m[0] + f[3] * m[2];
	unsigned long d = f[2] * m[1] + f[3] * m[3];
	f[0] = a;
	f[1] = b;
	f[2] = c;
	f[3] = d;
}

static const unsigned long m[4] = {1, 1, 1, 0};
static void power(unsigned long f[4], unsigned long n) {
	if (n > 1) {
		power(f, n / 2);
		multiply(f, f);
		if (n % 2 != 0)
			multiply(f, m);
	}
}

unsigned long fib(unsigned long value) {
	if (value < 2)
		return value;
	unsigned long f[4] = {1, 1, 1, 0};

		power(f, value - 1);
	return f[0];
}

int print_library_info(FILE *stream) {
	return fprintf(stream, "[using Fibonacci library v%u: O(log(n))]\n", version);
}
