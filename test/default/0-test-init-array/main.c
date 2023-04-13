#include <stdio.h>
#include <stdlib.h>

int i = 23;

__attribute__((constructor(142))) void init_func_x() {
	i *= 2;
}

__attribute__((constructor(123))) void init_func_y() {
	i -= 2;
}

int main() {
	// Check initialization using (obsolete) init section
	if (i == 23) {
		puts("Test failed: .init_array not executed");
		return EXIT_FAILURE;
	} else if (i == 42) {
		puts("Test successful");
		return EXIT_SUCCESS;
	} else {
		printf("Test failed: order of .init_array methods caused wrong value (%d)\n", i);
		return EXIT_FAILURE;
	}
}
