#include <stdio.h>
#include <stdlib.h>

int i = 2;

__attribute__((constructor(123))) void init_func_a() {
	i -= 2;
}

__attribute__((constructor(142))) void init_func_b() {
	i *= 2;
}

void init_func_c() {
	i += 13;
}

void init_func_d() {
	i *= 5;
}

asm(".section .init\n\t"
	"call init_func_c\n\t"
	".section .text\n\t");

__attribute__((section(".preinit_array")))
void (*preinit_array[])(void) = { &init_func_d };

int main() {
	// Check initialization using (obsolete) init section
	if (i == 0) {
		puts("Test failed: init methods not executed");
		return EXIT_FAILURE;
	} else if (i == 42) {
		puts("Test successful");
		return EXIT_SUCCESS;
	} else {
		printf("Test failed: order of init methods caused wrong value (%d)\n", i);
		return EXIT_FAILURE;
	}
}
