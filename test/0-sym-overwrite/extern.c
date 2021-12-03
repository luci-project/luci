#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned sleep(unsigned seconds) {
	printf("Should sleep for %u!\n", seconds);
	return seconds;
}

void no_sleep() {
	puts("Should not sleep!");
}
