#include "util.h"
#include "sys.h"

#define xstr(s) str(s)
#define str(s) #s
#ifndef VERSION
#define VERSION 0
#endif

const int version = VERSION;

enum LOG_LEVEL log_level = 2;

void sleep(int sec) {
#if VERSION >= 2
	if (sec <= 0)
		return;
#endif
	struct timespec rem, req = { .tv_sec = sec, .tv_nsec = 0 };
	while (sys_nanosleep(&req, &rem) != 0)
		req = rem;
}

size_t strlen(const char * str) {
	for (int i = 0; ; ++i )
		if (str[i] == '\0')
			return i;
}

void print(const char * str) {
#if VERSION >= 3
	if (str == NULL)
		return;
#endif
	sys_write(1, str, strlen(str));
}

#if VERSION >= 4
static void print_err(const char * str) {
	sys_write(2, str, strlen(str));
}
#endif

void log_message(enum LOG_LEVEL level, const char * str) {
	if (level >= log_level)
		return;
#if VERSION >= 3
	if (str == NULL)
		return;
#endif
#if VERSION >= 5
	switch (level) {
		case FATAL:   print_err("FATAL: ");   break;
		case ERROR:   print_err("ERROR: ");   break;
		case WARNING: print_err("WARNING: "); break;
		case INFO:    print_err("INFO: ");    break;
		case VERBOSE: print_err("VERBOSE: "); break;
		case TRACE:   print_err("TRACE: ");   break;
		default:      print_err("DEBUG: ");   break;
	}
#endif
#if VERSION < 4
	sys_write(2, str, strlen(str));
#else
	print_err(str);
#endif
}

void log_version() {
	log_message(VERBOSE, "Utility library v" str(VERSION));
}

#if VERSION >= 6
void die(const char * str) {
	log_message(FATAL, str);
#if VERSION >= 7
	sys_exit(1);
#else
	while(1) {}
#endif
}
#endif
