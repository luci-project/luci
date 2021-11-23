#pragma once

#include <time.h>

int sys_nanosleep(const struct timespec *req, struct timespec *rem);
int sys_write(int fd, const void *buf, size_t size);
void sys_exit(int code);
