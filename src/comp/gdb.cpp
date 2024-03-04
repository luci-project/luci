// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "comp/gdb.hpp"

#include <dlh/stream/string.hpp>
#include <dlh/syscall.hpp>
#include <dlh/assert.hpp>
#include <dlh/log.hpp>

GDB::RDebugExtended r_debug;

extern __attribute__((alias("r_debug"), visibility("default"))) GDB::RDebug _r_debug;

EXPORT void _dl_debug_state(void) {
	asm volatile("nop" ::: "memory");
}

namespace GDB {

/* When debugger support is enabled, this will create a flat list containing all versions of the object in the debug structure.
 * Hence GDB is able to resolve symbols in each version - and set breakpoints to all symbols having the same name. */
static List<GLIBC::DL::link_map, GLIBC::DL::link_map, &GLIBC::DL::link_map::l_next, &GLIBC::DL::link_map::l_prev> flat_link_map;
void refresh(const Loader & loader) {
	if (r_debug.base.r_brk != nullptr && loader.config.debugger) {
		auto i = flat_link_map.begin();
		for (const auto & o : loader.lookup)
			for (Object * c = o.current; c != nullptr; c = c->file_previous) {
				if (i == flat_link_map.end() || i->l_versions != c) {
					i = flat_link_map.insert(i, o.glibc_link_map);
					i->l_addr = c->base;
					if (c->data.fd < 0) {
						i->l_name = const_cast<char*>(o.path.str);
					} else {
						StringStream<100> procfsfd;
						procfsfd << "/proc/" << loader.pid << "/fd/" << c->data.fd;
						i->l_name = String::duplicate(procfsfd.str());
					}
					i->l_versions = reinterpret_cast<void*>(c);  // Hack. This field is reserved for symbol versioning, but we use it for object comparison instead.
				}
				++i;
			}
		r_debug.base.r_map = &flat_link_map.front();
	}
}

void notify(State state) {
	r_debug.base.r_state = state;
	if (r_debug.base.r_brk != nullptr)
		r_debug.base.r_brk();
}

static bool set_dynamic_debug(const ObjectIdentity * object) {
	assert(object != nullptr);
	if (object->dynamic == 0)
		return false;
	for (Elf::Dyn * dyn = reinterpret_cast<Elf::Dyn *>(object->dynamic); dyn->d_tag != Elf::DT_NULL; dyn++) {
		if (dyn->d_tag == Elf::DT_DEBUG) {
			Object * current = object->current;
			assert(current != nullptr);
			current->compose_pointer(dyn)->d_un.d_ptr = reinterpret_cast<uintptr_t>(&r_debug);
			current->finalize();
			LOG_INFO << "GDB debug structure in " << *object << " at *" << &(dyn->d_un.d_ptr) << " = " << &r_debug << endl;
			return true;
		}
	}
	return false;
}

void init(const Loader & loader) {
	r_debug.base.r_version = 1;
	r_debug.base.r_brk = _dl_debug_state;
	r_debug.base.r_state = State::RT_CONSISTENT;
	r_debug.base.r_ldbase = loader.self->current->data.addr;

	if (loader.config.debugger) {
		// Create initial flat copy
		refresh(loader);
	} else {
		// Just use the existing map (part of object identity)
		r_debug.base.r_map = &loader.lookup.front().glibc_link_map;
	}

	assert(r_debug.base.r_map != nullptr);

	// Set for Luci (in case it is executed from the command line)
	if (!set_dynamic_debug(loader.self))
		LOG_WARNING << "GDB debug structure not assigned in loader" << endl;

	// Set for target binary (in case Luci is started as interpreter)
	if (!set_dynamic_debug(&loader.lookup.front()))
		LOG_WARNING << "GDB debug structure not assigned in target" << endl;

	notify(State::RT_CONSISTENT);
}
}  // namespace GDB
