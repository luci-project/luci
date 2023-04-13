#include <stdio.h>
#include <stdlib.h>

#include "extern.h"

int main() {
	unsigned in = 3;
	unsigned out = sleep(in);
	printf("Remaining time: %u\n", out);
	if (in == out)
		no_sleep();

	return 0;
}
