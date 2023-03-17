#include <stdio.h>
#include <stdlib.h>

int i = 23;

void init_func_x() {
	i *= 2;
}

void init_func_y() {
	i -= 2;
}

__attribute__((section(".preinit_array")))
void (*preinit_array[])(void) = { &init_func_y, &init_func_x };

int main() {
	// Check initialization using (obsolete) init section
	if (i == 23) {
		puts("Test failed: .preinit_array not executed");
		return EXIT_FAILURE;
	} else if (i == 42) {
		puts("Test successful");
		return EXIT_SUCCESS;
	} else {
		printf("Test failed: order of .preinit_array methods caused wrong value (%d)\n", i);
		return EXIT_FAILURE;
	}
}
