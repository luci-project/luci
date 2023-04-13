#include "susfu.h"

#include "helper.h"
#include "fubar.h"

long susfu_num = 13;

static __attribute__((constructor(142))) void susfu_init_array_b() {
	TRACE_POINT();
	susfu_num -= 2;
}

static __attribute__((constructor(123))) void susfu_init_array_a() {
	TRACE_POINT();
	susfu_num *= 5;
}

void susfu_init_func() {
	TRACE_POINT();
	susfu_num += 4;
}

asm(".section .init\n\t"
	"call susfu_init_func\n\t"
	".section .text\n\t");


long susfu_func() {
	TRACE_POINT();
	return susfu_num * fubar_helper();
}
