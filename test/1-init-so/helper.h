#pragma once

#include <stddef.h>

struct InitTrace {
	const char * file;
	int line;
	const char * func;
};

#define TRACE_SIZE 64

extern struct InitTrace init_trace[TRACE_SIZE];

#define TRACE_POINT()                                                 \
	do {                                                              \
		int n = 0;                                                    \
		while (n < TRACE_SIZE - 1 && init_trace[n].file != NULL)      \
			n++;                                                      \
		init_trace[n].file = __FILE__;                                \
		init_trace[n].line = __LINE__;                                \
		init_trace[n].func = __FUNCTION__;                            \
	} while(0)
