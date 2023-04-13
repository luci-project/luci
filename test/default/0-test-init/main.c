#include <stdio.h>
#include <stdlib.h>

int i = 23;

void init_func_x() {
	i *= 2;
}

void init_func_y() {
	i -= 2;
}

asm(".section .init\n\t"
	"call init_func_y\n\t"
	"call init_func_x\n\t"
	".section .text\n\t");

int main() {
	// Check initialization using (obsolete) init section
	if (i == 23) {
		puts("Test failed: .init not executed");
		return EXIT_FAILURE;
	} else if (i == 42) {
		puts("Test successful");
		return EXIT_SUCCESS;
	} else {
		printf("Test failed: order of .init methods caused wrong value (%d)\n", i);
		return EXIT_FAILURE;
	}
}
