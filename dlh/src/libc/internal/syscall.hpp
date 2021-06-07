#pragma once

#include <cstddef>
#include <syscall.h>

#include <dlh/errno.hpp>

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

static long __syscall_ret(unsigned long r) {
	if (r > -4096UL) {
		errno = -r;
		return -1;
	}
	return r;
}

#define __scc(X) ((long) (X))
#define __syscall1(n,a) __syscall1(n,__scc(a))
#define __syscall2(n,a,b) __syscall2(n,__scc(a),__scc(b))
#define __syscall3(n,a,b,c) __syscall3(n,__scc(a),__scc(b),__scc(c))
#define __syscall4(n,a,b,c,d) __syscall4(n,__scc(a),__scc(b),__scc(c),__scc(d))
#define __syscall5(n,a,b,c,d,e) __syscall5(n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e))
#define __syscall6(n,a,b,c,d,e,f) __syscall6(n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e),__scc(f))

#define __SYSCALL_NARGS_X(a,b,c,d,e,f,g,h,n,...) n
#define __SYSCALL_NARGS(...) __SYSCALL_NARGS_X(__VA_ARGS__,7,6,5,4,3,2,1,0,)
#define __SYSCALL_CONCAT_X(a,b) a##b
#define __SYSCALL_CONCAT(a,b) __SYSCALL_CONCAT_X(a,b)
#define __SYSCALL_DISP(b,...) __SYSCALL_CONCAT(b,__SYSCALL_NARGS(__VA_ARGS__))(__VA_ARGS__)

#define __syscall(...) __SYSCALL_DISP(__syscall,__VA_ARGS__)
#define syscall(...) __syscall_ret(__syscall(__VA_ARGS__))
