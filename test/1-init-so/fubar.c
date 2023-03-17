#include "fubar.h"

#include "helper.h"
#include "susfu.h"

static int a, b = 7;

static __attribute__((constructor)) void fubar_init_array() {
	TRACE_POINT();
	b += 9;
	a--;
}

void fubar_init_func() {
	TRACE_POINT();
	a = b *= 10;
}

asm(".section .init\n\t"
	"call fubar_init_func\n\t"
	".section .text\n\t");


long fubar_helper() {
	TRACE_POINT();
	return a;
}

long fubar_func() {
	TRACE_POINT();
	return b * (susfu_func());
}
