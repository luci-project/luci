#include <stddef.h>
#include <syscall.h>
#include "sys.h"

static inline long __syscall0(long __n) {
	unsigned long __ret;
	asm volatile ("syscall" : "=a"(__ret) : "a"(__n) : "rcx", "r11", "memory");
	return __ret;
}

static inline long __syscall1(long __n, long __a1) {
	unsigned long __ret;
	asm volatile ("syscall" : "=a"(__ret) : "a"(__n), "D"(__a1) : "rcx", "r11", "memory");
	return __ret;
}

static inline long __syscall2(long __n, long __a1, long __a2) {
	unsigned long __ret;
	asm volatile ("syscall" : "=a"(__ret) : "a"(__n), "D"(__a1), "S"(__a2) : "rcx", "r11", "memory");
	return __ret;
}

static inline long __syscall3(long __n, long __a1, long __a2, long __a3) {
	unsigned long __ret;
	asm volatile ("syscall" : "=a"(__ret) : "a"(__n), "D"(__a1), "S"(__a2), "d"(__a3) : "rcx", "r11", "memory");
	return __ret;
}

static inline long __syscall4(long __n, long __a1, long __a2, long __a3, long __a4) {
	unsigned long __ret;
	register long __r10 __asm__("r10") = __a4;
	asm volatile ("syscall" : "=a"(__ret) : "a"(__n), "D"(__a1), "S"(__a2), "d"(__a3), "r"(__r10): "rcx", "r11", "memory");
	return __ret;
}

static inline long __syscall5(long __n, long __a1, long __a2, long __a3, long __a4, long __a5) {
	unsigned long __ret;
	register long __r10 __asm__("r10") = __a4;
	register long __r8 __asm__("r8") = __a5;
	asm volatile ("syscall" : "=a"(__ret) : "a"(__n), "D"(__a1), "S"(__a2), "d"(__a3), "r"(__r10), "r"(__r8) : "rcx", "r11", "memory");
	return __ret;
}

static inline long __syscall6(long __n, long __a1, long __a2, long __a3, long __a4, long __a5, long __a6) {
	unsigned long __ret;
	register long __r10 __asm__("r10") = __a4;
	register long __r8 __asm__("r8") = __a5;
	register long __r9 __asm__("r9") = __a6;
	asm volatile ("syscall" : "=a"(__ret) : "a"(__n), "D"(__a1), "S"(__a2), "d"(__a3), "r"(__r10), "r"(__r8), "r"(__r9) : "rcx", "r11", "memory");
	return __ret;
}

int sys_nanosleep(const struct timespec *req, struct timespec *rem) {
	return __syscall2(__NR_nanosleep, (long)req, (long)rem);
}

int sys_write(int fd, const void *buf, size_t size) {
	return __syscall3(__NR_write, (long)fd, (long)buf, (long)size);
}

void sys_exit(int code) {
	__syscall1(__NR_exit, (long)code);
	__builtin_unreachable();
}
