#include "comp/glibc/rtld/dl.hpp"

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

extern "C" __attribute__((__used__)) void __dl_mcount(uintptr_t from, uintptr_t to) {
	(void) from;
	(void) to;
	LOG_WARNING << "GLIBC _dl_mcount not implemented!" << endl;
}


asm(R"(
.globl _dl_mcount
.type _dl_mcount, @function
.align 16
_dl_mcount:
	.cfi_startproc
	endbr64

	# Save base pointer
	push %rax
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset rax, 0
	push %rcx
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset rcx, 0
	push %rdx
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset rdx, 0
	push %rsi
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset rsi, 0
	push %rdi
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset rdi, 0
	push %r8
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset r8, 0
	push %r9
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset r9, 0

	# Call high level mcount function
	call __dl_mcount

	# Restore register
	.cfi_adjust_cfa_offset -8
	pop %r9
	.cfi_adjust_cfa_offset -8
	pop %r8
	.cfi_adjust_cfa_offset -8
	pop %rdi
	.cfi_adjust_cfa_offset -8
	pop %rsi
	.cfi_adjust_cfa_offset -8
	pop %rdx
	.cfi_adjust_cfa_offset -8
	pop %rcx
	.cfi_adjust_cfa_offset -8
	pop %rax
	.cfi_adjust_cfa_offset -8

	.cfi_restore rax
	.cfi_restore rcx
	.cfi_restore rdx
	.cfi_restore rsi
	.cfi_restore rdi
	.cfi_restore r8
	.cfi_restore r9

	ret
	.cfi_endproc
)");


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
