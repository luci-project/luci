#include "tarfu.h"

#include "helper.h"

static volatile long x = -27;

static __attribute__((constructor)) void tarfu_init_array() {
	TRACE_POINT();
	x -= 3;
}

void tarfu_init_func_b() {
	TRACE_POINT();
	x *= 19;
}

static __attribute__((used)) void tarfu_init_func_a() {
	TRACE_POINT();
	x += 31;
}

asm(".section .init\n\t"
	"call tarfu_init_func_a\n\t"
	"call tarfu_init_func_b\n\t"
	".section .text\n\t");


long tarfu_func() {
	TRACE_POINT();
	return x;
}
