#pragma once

#include <dlh/types.hpp>
#include <cerrno>

#ifndef errno
#define errno (*__errno_location())
#endif

extern "C" int *__errno_location();

extern "C" char * strerror_r(int errnum, char *buf, size_t buflen);
extern "C" char * strerror(int errnum);
