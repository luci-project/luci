// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "comp/gdb.hpp"

#include <dlh/assert.hpp>
#include <dlh/log.hpp>

GDB::RDebugExtended r_debug;

extern __attribute__((alias("r_debug"), visibility("default"))) GDB::RDebug _r_debug;

EXPORT void _dl_debug_state(void) {
	asm volatile("nop" ::: "memory");
}

namespace GDB {

void notify(State state) {
	r_debug.base.r_state = state;
	if (r_debug.base.r_brk)
		r_debug.base.r_brk();
}

static bool set_dynamic_debug(const ObjectIdentity * object) {
	assert(object != nullptr);
	if (object->dynamic != 0) {
		for (auto dyn = reinterpret_cast<Elf::Dyn *>(object->dynamic); dyn->d_tag != Elf::DT_NULL; dyn++) {
			if (dyn->d_tag == Elf::DT_DEBUG) {
				auto current = object->current;
				assert(current != nullptr);
				current->compose_pointer(dyn)->d_un.d_ptr = reinterpret_cast<uintptr_t>(&r_debug);
				current->finalize();
				LOG_INFO << "GDB debug structure in " << *object << " at *" << &(dyn->d_un.d_ptr) << " = " << &r_debug << endl;
				return true;
			}
		}
	}
	return false;
}

void init(const Loader & loader) {
	r_debug.base.r_version = 1;
	r_debug.base.r_map = &loader.lookup.front();
	r_debug.base.r_brk = _dl_debug_state;
	r_debug.base.r_state = State::RT_CONSISTENT;
	r_debug.base.r_ldbase = loader.self->current->data.addr;

	assert(r_debug.base.r_map != nullptr);

	// Set for Luci (in case it is executed from the command line)
	if (!set_dynamic_debug(loader.self))
		LOG_WARNING << "GDB debug structure not assigned in loader" << endl;

	// Set for target binary (in case Luci is started as interpreter)
	if (!set_dynamic_debug(r_debug.base.r_map))
		LOG_WARNING << "GDB debug structure not assigned in target" << endl;

	notify(State::RT_CONSISTENT);
}
}  // namespace GDB
