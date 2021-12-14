#include <stddef.h>
#include <sys/types.h>
#include <syscall.h>


static int sys_nanosleep(const struct timespec *req, struct timespec *rem) {
	unsigned long r;
	asm volatile ("syscall" : "=a"(r) : "a"(__NR_nanosleep), "D"(req), "S"(rem) : "rcx", "r11", "memory");
	return r;
}

static void sleep(int sec) {
	if (sec > 0) {
		struct timespec rem, req = { .tv_sec = sec, .tv_nsec = 0 };
		while (sys_nanosleep(&req, &rem) != 0)
			req = rem;
	}
}

static int sys_open(const char *pathname, int flags) {
	long fd;
	asm volatile ("syscall" : "=a"(fd) : "a"(__NR_open), "D"(pathname), "S"(flags) : "rcx", "r11", "memory");
	return fd;
}

static ssize_t sys_read(int fd, const char * msg, size_t len) {
	long r;
	asm volatile ("syscall" : "=a"(r) : "a"(__NR_read), "D"(fd), "S"(msg), "d"(len) : "rcx", "r11", "memory");
	return r;
}

static ssize_t sys_write(int fd, const char * msg, size_t len) {
	long r;
	asm volatile ("syscall" : "=a"(r) : "a"(__NR_write), "D"(fd), "S"(msg), "d"(len) : "rcx", "r11", "memory");
	return r;
}

static int sys_close(int fd) {
	long r;
	asm volatile ("syscall" : "=a"(r) : "a"(__NR_close), "D"(fd) : "rcx", "r11", "memory");
	return r;
}

static void sys_exit(int code) {
	asm volatile ("syscall" :: "a"(__NR_exit), "D"(code) : "rcx", "r11", "memory");
	__builtin_unreachable();
}
