#include "gdb.hpp"

#include <dlh/assert.hpp>
#include <dlh/utils/log.hpp>

/* Rendezvous structure used by the run-time dynamic linker to communicate
   details of shared object loading to the debugger.  If the executable's
   dynamic section has a DT_DEBUG element, the run-time linker sets that
   element's value to the address where this structure can be found.  */
struct r_debug {
	int r_version;		/* Version number for this protocol.  */

	const ObjectIdentity *r_map;	/* Head of the chain of loaded objects.  */

	/* This is the address of a function internal to the run-time linker,
	   that will always be called when the linker begins to map in a
	   library or unmap it, and again when the mapping change is complete.
	   The debugger can set a breakpoint at this address if it wants to
	   notice shared object mapping changes.  */
	void (* r_brk)();

	/* This state value describes the mapping change taking place when
	   the `r_brk' address is called.  */

	enum {
		RT_CONSISTENT,   /* Mapping change is complete.  */
		RT_ADD,          /* Beginning to add a new object.  */
		RT_DELETE        /* Beginning to remove an object mapping.  */
	} r_state;

	uintptr_t r_ldbase;  /* Base address the linker is loaded at.  */
} _r_debug;



void gdb_notify() {
	asm volatile ("nop" ::: "memory");
}

void gdb_initialize(const Loader & loader) {
	_r_debug.r_version = 1;
	_r_debug.r_map = &loader.lookup.front();
	_r_debug.r_brk = gdb_notify;
	_r_debug.r_state = r_debug::RT_CONSISTENT;
	_r_debug.r_ldbase = loader.self->base;
	assert(_r_debug.r_map != nullptr);
	if (_r_debug.r_map->dynamic != 0) {
		assert(_r_debug.r_map->current != nullptr);
		for (auto dyn = reinterpret_cast<Elf::Dyn *>(_r_debug.r_map->dynamic); dyn->d_tag != Elf::DT_NULL; dyn++)
			if (dyn->d_tag == Elf::DT_DEBUG) {
				dyn->d_un.d_ptr = reinterpret_cast<uintptr_t>(&_r_debug);
				LOG_INFO << "GDB debug structure at *" << &(dyn->d_un.d_ptr) << " = " << &_r_debug << endl;
				return;
			}
	}
	LOG_WARNING << "GDB debug structure not assigned" << endl;
}
