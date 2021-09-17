#include "compatibility/gdb.hpp"

#include <dlh/assert.hpp>
#include <dlh/log.hpp>

/* Rendezvous structure used by the run-time dynamic linker to communicate
   details of shared object loading to the debugger.  If the executable's
   dynamic section has a DT_DEBUG element, the run-time linker sets that
   element's value to the address where this structure can be found.  */
struct RDebug{
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

	GDB::State r_state;

	uintptr_t r_ldbase;  /* Base address the linker is loaded at.  */
} r_debug;

extern __attribute__ ((alias("r_debug"), visibility("default"))) RDebug _r_debug;

EXPORT void _dl_debug_state(void) {
	asm volatile("nop" ::: "memory");
}

namespace GDB {

void notify(State state) {
	r_debug.r_state = state;
	if (r_debug.r_brk)
		r_debug.r_brk();
}

static bool set_dynamic_debug(const ObjectIdentity * object) {
	if (object->dynamic != 0)
		for (auto dyn = reinterpret_cast<Elf::Dyn *>(object->dynamic); dyn->d_tag != Elf::DT_NULL; dyn++)
			if (dyn->d_tag == Elf::DT_DEBUG) {
				dyn->d_un.d_ptr = reinterpret_cast<uintptr_t>(&r_debug);
				LOG_INFO << "GDB debug structure in " << *object << " at *" << &(dyn->d_un.d_ptr) << " = " << &r_debug << endl;
				return true;
			}
	return false;
}

void init(const Loader & loader) {
	r_debug.r_version = 1;
	r_debug.r_map = &loader.lookup.front();
	r_debug.r_brk = _dl_debug_state;
	r_debug.r_state = State::RT_CONSISTENT;
	r_debug.r_ldbase = loader.self->base;

	assert(r_debug.r_map != nullptr);

	// Set for Luci (in case it is executed from the command line)
	if (!set_dynamic_debug(loader.self))
		LOG_WARNING << "GDB debug structure not assigned in loader" << endl;

	// Set for target binary (in case Luci is started as interpreter)
	if (!set_dynamic_debug(r_debug.r_map))
		LOG_WARNING << "GDB debug structure not assigned in target" << endl;
}
}  // namespace GDB
