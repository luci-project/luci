#include "bohica.h"

#include "fubar.h"
#include "helper.h"

int bohica_num = 10;

static __attribute__((constructor)) void bohica_init_array() {
	TRACE_POINT();
	bohica_num += 7;
}

void bohica_init_func() {
	TRACE_POINT();
	bohica_num *= 6;
}

asm(".section .init\n\t"
	"call bohica_init_func\n\t"
	".section .text\n\t");

long bohica_func() {
	TRACE_POINT();
	return bohica_num * fubar_func();
}
