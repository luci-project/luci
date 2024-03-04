// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include "loader.hpp"

namespace GDB {
enum State {
	RT_CONSISTENT,   /* Mapping change is complete.  */
	RT_ADD,          /* Beginning to add a new object.  */
	RT_DELETE        /* Beginning to remove an object mapping.  */
};

/* Rendezvous structure used by the run-time dynamic linker to communicate
   details of shared object loading to the debugger.  If the executable's
   dynamic section has a DT_DEBUG element, the run-time linker sets that
   element's value to the address where this structure can be found.  */
struct RDebug {
	int r_version;		/* Version number for this protocol.  */

	const GLIBC::DL::link_map *r_map;	/* Head of the chain of loaded objects.  */

	/* This is the address of a function internal to the run-time linker,
	   that will always be called when the linker begins to map in a
	   library or unmap it, and again when the mapping change is complete.
	   The debugger can set a breakpoint at this address if it wants to
	   notice shared object mapping changes.  */
	void (* r_brk)();

	/* This state value describes the mapping change taking place when
	   the `r_brk' address is called.  */

	State r_state;

	uintptr_t r_ldbase;  /* Base address the linker is loaded at.  */
};

/* The extended rendezvous structure used by the run-time dynamic linker
   to communicate details of shared object loading to the debugger.  If
   the executable's dynamic section has a DT_DEBUG element, the run-time
   linker sets that element's value to the address where this structure
   can be found.  */
struct RDebugExtended {
	RDebug base;
	/* Link to the next r_debug_extended structure.  Each r_debug_extended
	   structure represents a different namespace.  The first
	   r_debug_extended structure is for the default namespace.  */
	RDebugExtended *r_next;
};


void init(const Loader & loader);
void refresh(const Loader & loader);
void notify(State state = RT_CONSISTENT);
static inline void breakpoint() {
	asm volatile("int3" ::: "memory");
}

}  // namespace GDB
