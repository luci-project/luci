// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

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
