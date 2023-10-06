#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "fib.h"

int main(int argc, const char * argv[]) {
	// First parameter can specify end
	unsigned long to = argc < 2 ? ULONG_MAX : strtoul(argv[1], NULL, 0);

	// Iterate over range
	for (unsigned long i = 0; i < to; i++) {
		// Call library function and print result
		printf("fib(%lu) = %lu\n", i, fib(i));

		// For i > 93 64bit is not enough.
		if (i == 93)
			puts("(last valid number)");

		// Dump libray info on standard output
		print_library_info(stdout);

#if defined(DELAY) && DELAY > 0
		// Wait DELAY seconds to relax the CPU (and our eyes)
		sleep(DELAY);
#endif
	}
	return 0;
}

