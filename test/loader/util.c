#include "util.h"
#include "sys.h"

void sleep(int sec) {
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
	if (str != NULL)
		sys_write(1, str, strlen(str));
}
