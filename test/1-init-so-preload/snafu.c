#include "snafu.h"

#include "helper.h"
#include "fubar.h"

static long snafu_num = 5;

static __attribute__((constructor)) void snafu_init_array() {
	TRACE_POINT();
	snafu_num += 16;
}

void snafu_init_func() {
	TRACE_POINT();
	snafu_num *= 11;
}

asm(".section .init\n\t"
	"call snafu_init_func\n\t"
	".section .text\n\t");

long snafu_func() {
	TRACE_POINT();
	return snafu_num * fubar_helper();
}
