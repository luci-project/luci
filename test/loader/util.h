#pragma once

#include <stddef.h>

enum LOG_LEVEL {
	FATAL   = 0,
	ERROR   = 1,
	WARNING = 2,
	INFO    = 3,
	VERBOSE = 4,
	TRACE   = 5,
	DEBUG   = 6
};

extern enum LOG_LEVEL log_level;

extern const int version;

void sleep(int);

size_t strlen(const char *);

void print(const char *);

void log_message(enum LOG_LEVEL, const char *);

void log_version();

#if VERSION >= 6
void die(const char *);
#endif
