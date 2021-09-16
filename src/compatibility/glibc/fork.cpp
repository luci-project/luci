#include <dlh/log.hpp>
#include <dlh/types.hpp>
#include <dlh/assert.hpp>
#include <dlh/thread.hpp>

#include "loader.hpp"

extern "C" __attribute__((__used__)) int __fork_syscall() {
#ifndef NO_FPU
	const uint32_t mask_low = 0xff;  // assume XSAVE & AVX, TODO: Use CPUID
	const uint32_t mask_high = 0;
	alignas(64) uint8_t buf[4096] = {};
	asm volatile ("xsave (%0)" : : "r"(buf), "a"(mask_low), "d"(mask_high) : "%mm0", "%ymm0", "memory" );
#endif
	LOG_ERROR << "Fork Syscall" << endl;

	auto loader = Loader::instance();
	assert(loader != nullptr);

	// TODO: Duplicate memfd

	pid_t child = 0;
	int r = -1;
	if (auto clone = Syscall::clone(CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID | SIGCHLD, 0, NULL, &child, 0)) {
		Thread::self()->tid = child;

		if ((r = clone.value()) == 0) {
			// set (map) memfd copies
		}
	}


#ifndef NO_FPU
	asm volatile ("xrstor (%0)" : : "r"(buf), "a"(mask_low), "d"(mask_high) : "%mm0", "%ymm0", "memory" );
#endif
	return r;
}


asm(R"(
.globl _fork_syscall
.hidden _fork_syscall
.type _fork_syscall, @function
.align 16
_fork_syscall:
	.cfi_startproc
	endbr64

	# Save base pointer
	push %rbp
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset rbp, 0
	push %rbx
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset rbx, 0
	push %rcx
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset rcx, 0
	push %r8
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset r8, 0
	push %r9
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset r9, 0
	push %r10
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset r10, 0
	push %r11
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset r11, 0
	push %r12
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset r12, 0
	push %r13
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset r13, 0
	push %r14
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset r14, 0
	push %r15
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset r15, 0

	# Call high level fork function
	call __fork_syscall
	# Values of fork syscall
	mov $0, %rdx
	mov $0, %rsi
	mov $0x1200011, %rdi

	# Restore register
	pop %r15
	.cfi_adjust_cfa_offset -8
	pop %r14
	.cfi_adjust_cfa_offset -8
	pop %r13
	.cfi_adjust_cfa_offset -8
	pop %r12
	.cfi_adjust_cfa_offset -8
	pop %r11
	.cfi_adjust_cfa_offset -8
	pop %r10
	.cfi_adjust_cfa_offset -8
	pop %r9
	.cfi_adjust_cfa_offset -8
	pop %r8
	.cfi_adjust_cfa_offset -8
	pop %rcx
	.cfi_adjust_cfa_offset -8
	pop %rbx
	.cfi_adjust_cfa_offset -8
	pop %rbp
	.cfi_adjust_cfa_offset -8

	.cfi_restore rbp
	.cfi_restore rbx
	.cfi_restore rcx
	.cfi_restore r8
	.cfi_restore r9
	.cfi_restore r10
	.cfi_restore r11
	.cfi_restore r12
	.cfi_restore r13
	.cfi_restore r14
	.cfi_restore r15

	ret
	.cfi_endproc
)");
