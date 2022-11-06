#include "compatibility/glibc/rtld/exception.hpp"

#include <dlh/log.hpp>
#include <dlh/macro.hpp>
#include <dlh/syscall.hpp>

EXPORT void _dl_exception_create(dl_exception *exception, const char *objname, const char *errstring) {
	(void) exception;
	(void) objname;
	(void) errstring;
	LOG_WARNING << "GLIBC _dl_exception_create not implemented!" << endl;
}

EXPORT void _dl_exception_create_format(dl_exception *exception, const char *objname, const char *fmt, ...) {
	(void) exception;
	(void) objname;
	(void) fmt;
	LOG_WARNING << "GLIBC _dl_exception_create_format not implemented!" << endl;
}

EXPORT void _dl_exception_free(dl_exception *exception) {
	(void) exception;
	LOG_WARNING << "GLIBC _dl_exception_free not implemented!" << endl;
}

EXPORT void _dl_signal_exception(int errcode, dl_exception *exception, const char *occasion) {
	LOG_WARNING << "GLIBC _dl_signal_exception not implemented!" << endl;
	LOG_ERROR << "Exception signal " << errcode
	          << " in " << exception->objname
	          << " at " << occasion
	          << ": " << exception->errstring
	          << endl;
}


EXPORT int _dl_catch_exception (dl_exception *exception, void (*operate) (void *), void *args) {
	(void) exception;
	(void) operate;
	(void) args;
	LOG_WARNING << "GLIBC _dl_catch_exception not implemented!" << endl;
	return 0;
}

EXPORT void _dl_signal_error(int errcode, const char *objname, const char *occasion, const char *errstring) {
	LOG_WARNING << "GLIBC _dl_signal_error not implemented!" << endl;
	LOG_ERROR << "Error signal " << errcode
	          << " in " << objname
	          << " at " << occasion
	          << ": " << errstring
	          << endl;
}

EXPORT int _dl_catch_error (const char **objname, const char **errstring, bool *mallocedp, void (*operate) (void *), void *args) {
	(void) objname;
	(void) errstring;
	(void) mallocedp;
	(void) operate;
	(void) args;
	LOG_WARNING << "GLIBC _dl_catch_error not implemented!" << endl;
	return 0;
}

int _dl_check_caller (const void *caller, int mask) {
	(void) caller;
	(void) mask;
	LOG_WARNING << "GLIBC _dl_check_caller not implemented!" << endl;
	return 0;
}


EXPORT void _dl_fatal_printf(const char *fmt, ...) {
	va_list arg;
	va_start (arg, fmt);
	LOG_DEBUG.output(fmt, arg);
	va_end (arg);
	Syscall::exit(127);
}
