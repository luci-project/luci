#include "preload.h"

#include "helper.h"
#include "snafu.h"
#include "fubar.h"

static int num = 5;

static __attribute__((constructor)) void preload_init_array() {
	TRACE_POINT();
	num += 27;
}

void preload_init_func() {
	TRACE_POINT();
	num *= 12;
}

asm(".section .init\n\t"
	"call preload_init_func\n\t"
	".section .text\n\t");


long preload_func() {
	TRACE_POINT();
	return num * (snafu_func() + fubar_func());
}

long tarfu_func() {
	TRACE_POINT();
	return preload_func();
}
