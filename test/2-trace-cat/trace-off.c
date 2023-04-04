#include "sys.h"

int close(int fd) {
	sleep(4);
	return sys_close(fd);
}

void exit(int code) {
	sys_exit(code);
	__builtin_unreachable();
}

int open(const char *pathname, int flags) {
	return sys_open(pathname, flags);
}

ssize_t read(int fd, const char * msg, size_t len) {
	return sys_read(fd, msg, len);
}

ssize_t write(int fd, const char * msg, size_t len) {
	return sys_write(fd, msg, len);
}
