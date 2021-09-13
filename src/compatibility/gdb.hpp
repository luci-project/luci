#pragma once

#include "loader.hpp"

namespace GDB {
enum State {
	RT_CONSISTENT,   /* Mapping change is complete.  */
	RT_ADD,          /* Beginning to add a new object.  */
	RT_DELETE        /* Beginning to remove an object mapping.  */
};
void init(const Loader & loader);
void notify(State state = RT_CONSISTENT);
static inline void breakpoint() {
	asm volatile("int3" ::: "memory");
}
}  // namespace GDB
