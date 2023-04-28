// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

struct dl_exception {
	const char *objname;
	const char *errstring;
	char *message_buffer;
};

// extern "C" void _dl_exception_create(dl_exception *exception, const char *objname, const char *errstring):
extern "C" void _dl_exception_create_format(dl_exception *exception, const char *objname, const char *fmt, ...);
extern "C" void _dl_exception_free(dl_exception *exception);
extern "C" void _dl_signal_exception(int errcode, dl_exception *exception, const char *occasion);
extern "C" int _dl_catch_exception (dl_exception *exception, void (*operate)(void *), void *args);
extern "C" void _dl_signal_error(int errcode, const char *objname, const char *occasion, const char *errstring);
extern "C" int _dl_catch_error(const char **objname, const char **errstring, bool *mallocedp, void (*operate)(void *), void *args);
extern "C" int _dl_check_caller(const void *caller, int mask);
extern "C" void _dl_error_free(void *ptr);

extern "C" void _dl_fatal_printf(const char *fmt, ...);
