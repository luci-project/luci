#pragma once

// Based on musl libc (and rarely glibc)


#include <dlh/errno.hpp>
#include <dlh/types.hpp>

#include <csignal>

#define _FCNTL_H
#include <bits/fcntl.h>
#include <bits/types.h>
#include <bits/types/struct_timespec.h>

//#include <sys/inotify.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/mman.h>

extern "C" pid_t gettid();
extern "C" pid_t getpid();
extern "C" pid_t getppid();

extern "C" int getrlimit(int resource, struct rlimit *rlim);
extern "C" int arch_prctl(int code, unsigned long addr);

extern "C" int raise(int sig);
extern "C" [[noreturn]] void abort();
extern "C" [[noreturn]] void exit(int code);

extern "C" void *mmap(void *start, size_t len, int prot, int flags, int fd, long off);
extern "C" int mprotect(void *addr, size_t len, int prot);
extern "C" int munmap(void *start, size_t len);
extern "C" int msync(void *start, size_t len, int flags);

extern "C" int access(const char *filename, int amode);
extern "C" int open(const char *filename, int flags);
extern "C" ssize_t read(int fd, void *buf, size_t count);
extern "C" ssize_t write(int fd, const void *buf, size_t size);
extern "C" int close(int fd);
extern "C" int fcntl(int fd, int cmd, unsigned long arg = 0);
extern "C" int fallocate(int fd, int mode, off_t base, off_t len);
extern "C" int ftruncate(int fd, off_t length);

extern "C" int fstat(int fd, struct stat *st);
extern "C" int stat(const char * __restrict__ path, struct stat * __restrict__ buf);
extern "C" int lstat(const char * __restrict__ path, struct stat * __restrict__ buf);

extern "C" int memfd_create(const char *name, unsigned flags);

extern "C" int inotify_init();
extern "C" int inotify_init1(int flags);
extern "C" int inotify_add_watch(int fd, const char *pathname, uint32_t mask);
extern "C" int inotify_rm_watch(int fd, int wd);

extern "C" int futex(int * __restrict__ uaddr, int futex_op, int val, const void * __restrict__ timeout, int * __restrict__ uaddr2, int val3);

extern "C" int brk(void * addr) __attribute__((weak));
extern "C" void * sbrk(intptr_t inc) __attribute__((weak));
