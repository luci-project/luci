#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "fib.h"

// Use clock measuring threads CPU time
static const clockid_t clkid = CLOCK_MONOTONIC_RAW;

int main(int argc, const char * argv[]) {
	// First parameter can specify end
	unsigned long to = argc < 2 ? ULONG_MAX : strtoul(argv[1], NULL, 0);

	// Iterate over range
	for (unsigned long i = 0; i < to; i++) {
		// Take start time
		struct timespec start, end;
		clock_gettime(clkid, &start);

		// Call library function
		unsigned long r =  fib(i);
		
		// Take end time and calculate duration
		clock_gettime(clkid, &end);
		struct timespec dur = {
			end.tv_sec - start.tv_sec,
			end.tv_nsec - start.tv_nsec
		};
		if (dur.tv_nsec < 0) {
			dur.tv_sec--;
			dur.tv_nsec += 1000000000;
		}

		// Output value and duration
		printf("fib(%lu) = %lu (in %lu.%06lus)\n", i, r, dur.tv_sec, (dur.tv_nsec + 500) / 1000);

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

