#include <dlh/log.hpp>
#include <dlh/macro.hpp>

EXPORT void _dl_audit_preinit(__attribute__((unused)) void *l) {
	(void) l;
	LOG_WARNING << "GLIBC _dl_audit_preinit not implemented!" << endl;
}

EXPORT void _dl_audit_symbind_alt(void *l, const void *ref, void **value, void* result) {
	(void) l;
	(void) ref;
	(void) value;
	(void) result;
	LOG_WARNING << "GLIBC _dl_audit_symbind_alt not implemented!" << endl;
}
