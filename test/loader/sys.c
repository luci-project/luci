#include <stddef.h>
#include <syscall.h>
#include "sys.h"

int sys_write(int fd, const void *buf, size_t size) {
	long result;
	asm volatile (
		"syscall"
		: "=a"(result)
		: "0"(__NR_write), "D"(fd), "S"(buf), "d"(size)
		: "cc", "rcx", "r11", "memory"
	);
	return result;
}

void sys_exit(int code) {
	asm volatile (
		"syscall"
		:
		: "a"(__NR_exit)
		: "cc", "rcx", "r11", "memory"
	);
	__builtin_unreachable();
}
