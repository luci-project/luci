#include <unistd.h>
#include <stdio.h>

#include "fib.h"

int main() {
	puts("[C main]");
	for (long i = 0; i < 3; i++) {
		if (i)
			sleep(10);
		printf("fib(%ld) = %ld\n", i, fib(i));
		fflush(stdout);
		printfib(21 + i);
	}
	return 0;
}
