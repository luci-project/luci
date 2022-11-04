#include "compatibility/glibc/rtld/dl.hpp"

#include <dlh/log.hpp>
#include <dlh/macro.hpp>
#include <dlh/syscall.hpp>

#include "loader.hpp"


int dl_starting_up = 0;
extern __attribute__ ((alias("dl_starting_up"), visibility("default"))) int _dl_starting_up;

__attribute__ ((visibility("default"))) char **_dl_argv = nullptr;

namespace GLIBC {
namespace RTLD {
void starting() {
	dl_starting_up = 1;
}
}  // namespace RTLD
}  // namespace GLIBC

EXPORT ObjectIdentity *_dl_find_dso_for_object(uintptr_t addr) {
	LOG_TRACE << "GLIBC _dl_find_dso_for_object( " << addr << ")" << endl;
	auto loader = Loader::instance();
	assert(loader != nullptr);

	GuardedReader _{loader->lookup_sync};
	auto o = loader->resolve_object(reinterpret_cast<uintptr_t>(addr));
	if (o != nullptr)
		return &(o->file);
	return nullptr;
}

EXPORT int _dl_make_stack_executable(__attribute__((unused)) void **stack_endp) {
	LOG_WARNING << "GLIBC _dl_make_stack_executable not implemented!" << endl;
	return 0;
}

EXPORT void _dl_mcount(__attribute__((unused)) uintptr_t from, __attribute__((unused)) uintptr_t to) {
	LOG_WARNING << "GLIBC _dl_mcount not implemented!" << endl;
}

EXPORT void __rtld_version_placeholder() {
	/* do nothing */
}

__attribute__ ((visibility("default"))) bool __nptl_initial_report_events = false;

EXPORT int __nptl_change_stack_perm (Thread *t) {
	uintptr_t stack = reinterpret_cast<uintptr_t>(t->stackblock) + t->guardsize;
	size_t len = t->stackblock_size - t->guardsize;
	return Syscall::mprotect(stack, len, PROT_READ | PROT_WRITE | PROT_EXEC).error();
}

EXPORT void __libdl_freeres() {
	LOG_WARNING << "GLIBC __libdl_freeres not implemented!" << endl;
}
