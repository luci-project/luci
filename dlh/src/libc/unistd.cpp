#include <dlh/unistd.hpp>

#include "internal/syscall.hpp"

//TODO use sysconf
#define PAGE_SIZE 4096

extern "C" pid_t gettid() {
	return syscall(__NR_gettid);
}

extern "C" pid_t getpid() {
	return syscall(__NR_getpid);
}

extern "C" pid_t getppid() {
	return syscall(__NR_getppid);
}

extern "C" int getrlimit(int resource, struct rlimit *rlim) {
	unsigned long k_rlim[2];
	int ret = syscall(__NR_prlimit64, 0, resource, 0, rlim);
	if (!ret || errno != ENOSYS)
		return ret;
	if (syscall(__NR_getrlimit, resource, k_rlim) < 0)
		return -1;
	rlim->rlim_cur = k_rlim[0] == -1UL ? (~0ULL) : k_rlim[0];
	rlim->rlim_max = k_rlim[1] == -1UL ? (~0ULL) : k_rlim[1];
	return 0;
}

extern "C" int arch_prctl(int code, unsigned long addr) {
	return syscall(__NR_arch_prctl, code, addr);
}


extern "C" int raise(int sig) {
	unsigned long set, mask = { 0xfffffffc7fffffff };
	__syscall(__NR_rt_sigprocmask, SIG_BLOCK, &mask, &set, 8);
	int ret = syscall(SYS_tkill, gettid(), &sig);
	__syscall(__NR_rt_sigprocmask, SIG_SETMASK, &set, 0, 8);
	return ret;
}


extern "C" [[noreturn]] void abort() {
	raise(SIGABRT);
	raise(SIGKILL);
	while(1) {}
}

extern "C" [[noreturn]] void exit(int code) {
	syscall(__NR_exit, (long)code);
	__builtin_unreachable();
}


extern "C" void *mmap(void *start, size_t len, int prot, int flags, int fd, long off) {
	return (void *)syscall(__NR_mmap, start, len, prot, flags, fd, off);
}

extern "C" int mprotect(void *addr, size_t len, int prot) {
	size_t start = (size_t)addr & -PAGE_SIZE;
	size_t end = (size_t)((char *)addr + len + PAGE_SIZE - 1) & -PAGE_SIZE;
	return syscall(__NR_mprotect, start, end-start, prot);
}

extern "C" int munmap(void *start, size_t len) {
	return syscall(__NR_munmap, start, len);
}

extern "C" int msync(void *start, size_t len, int flags) {
	return syscall(__NR_msync, start, len, flags);
}



extern "C" int access(const char *filename, int amode) {
	int r = __syscall(__NR_access, filename, amode);
	if (r == -ENOSYS)
		r = __syscall(__NR_faccessat, AT_FDCWD, filename, amode, 0);
	return __syscall_ret(r);
}

extern "C" int open(const char *filename, int flags) {
	int fd = __syscall(__NR_open, filename, flags);
	if (fd >= 0 && (flags & O_CLOEXEC))
		fcntl(fd, F_SETFD, FD_CLOEXEC);

	return __syscall_ret(fd);
}

extern "C" ssize_t read(int fd, void *buf, size_t count) {
	return syscall(__NR_read, fd, buf, count);
}

extern "C" ssize_t write(int fd, const void *buf, size_t size) {
	return syscall(__NR_write, fd, buf, size);
}

extern "C" int close(int fd) {
	return syscall(__NR_close, fd);
}

extern "C" int fcntl(int fd, int cmd, unsigned long arg) {
	switch (cmd) {
		case F_GETOWN:
		case F_DUPFD_CLOEXEC:
			return __syscall_ret(-ENOSYS);
		default:
			return syscall(__NR_fcntl, fd, cmd, arg);
	}
}

extern "C" int fallocate(int fd, int mode, off_t base, off_t len) {
	return syscall(__NR_fallocate, fd, mode, base, len);
}

extern "C" int ftruncate(int fd, off_t length) {
	return syscall(__NR_ftruncate, fd, length);
}


extern "C" int fstat(int fd, struct stat *st) {
	return syscall(__NR_fstat, fd, st);
}

extern "C" int stat(const char * __restrict__ path, struct stat * __restrict__ buf) {
	return syscall(__NR_stat, path, buf);
}

extern "C" int lstat(const char * __restrict__ path, struct stat * __restrict__ buf) {
	return syscall(__NR_lstat, path, buf);
}


extern "C" int memfd_create(const char *name, unsigned flags) {
	return syscall(__NR_memfd_create, name, flags);
}


extern "C" int inotify_init() {
	return inotify_init1(0);
}

extern "C" int inotify_init1(int flags) {
	int r = __syscall(__NR_inotify_init1, flags);
	if (r == -ENOSYS && !flags)
		r = __syscall(__NR_inotify_init);
	return __syscall_ret(r);
}

extern "C" int inotify_add_watch(int fd, const char *pathname, uint32_t mask) {
	return syscall(__NR_inotify_add_watch, fd, pathname, mask);
}

extern "C" int inotify_rm_watch(int fd, int wd) {
	return syscall(__NR_inotify_rm_watch, fd, wd);
}

extern "C"  int futex(int * __restrict__ uaddr, int futex_op, int val, const void * __restrict__ timeout, int * __restrict__ uaddr2, int val3) {
	return syscall(__NR_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}


static void * curbrk = 0;

extern "C" int brk(void *addr) {
	curbrk = (void *)syscall(__NR_brk, addr);
	return curbrk < addr ? __syscall_ret(-ENOMEM) : 0;
}

extern "C" void *sbrk(intptr_t inc) {
	if (curbrk == NULL)
		if (brk (0) < 0)
			return (void *) -1;

	void * o = curbrk;
	if (inc == 0)
		return o;
	else if ((inc > 0 && ((uintptr_t) o + (uintptr_t) inc < (uintptr_t) o)) ||
	         (inc < 0 && ((uintptr_t) o < (uintptr_t) -inc)))
		return  (void*)__syscall_ret(-ENOMEM);

	return brk((void*)((uintptr_t)o + inc)) < 0 ? (void *) -1 : o;
}
