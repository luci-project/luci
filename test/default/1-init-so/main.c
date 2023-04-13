#include <stdio.h>
#include <stdlib.h>

#include "helper.h"
#include "fubar.h"
#include "tarfu.h"

struct InitTrace init_trace[TRACE_SIZE] = { { __FILE__, __LINE__, "[start]" } };

__attribute__((constructor(142))) void main_init_array_c() {
	TRACE_POINT();
}

__attribute__((constructor(123))) void main_init_array_a() {
	TRACE_POINT();
}

void main_init_func() {
	TRACE_POINT();
}

void main_preinit_array_b() {
	TRACE_POINT();
}

static __attribute__((constructor(137))) void main_init_array_b() {
	TRACE_POINT();
}

static void main_preinit_array_a() {
	TRACE_POINT();
}

asm(".section .init\n\t"
	"call main_init_func\n\t"
	".section .text\n\t");

__attribute__((section(".preinit_array")))
void (*preinit_array[])(void) = { &main_preinit_array_a,  &main_preinit_array_b };

int main() {
	TRACE_POINT();
	printf("fubar_func: %ld\n", fubar_func());
	printf("tarfu_func: %ld\n", tarfu_func());
	TRACE_POINT();
	// dlopen mit existierenden symbol wegen overwrite
	for (int i = 0; i < TRACE_SIZE && init_trace[i].file != NULL; i++)
		printf("%2d. %s (%s:%d)\n", i, init_trace[i].func, init_trace[i].file, init_trace[i].line);
	return 0;
}

/*
libsnafu
libtarfu
libsusfu
libfubar
libbohica

libfubu

 libpreload -> libsnafu -> libtarfu
            -> libfubar
 libbohica -> libfubar
 main -> libfubar -> libsusfu
      -> libtarfu
*/
