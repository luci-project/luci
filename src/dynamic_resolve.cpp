// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "dynamic_resolve.hpp"

#include "loader.hpp"
#include "object/base.hpp"

extern "C" __attribute__((__used__)) void * __dlresolve(const Object & o, size_t index) {
#ifndef NO_FPU
	alignas(64) uint8_t buf[4096] = {};
	const uint32_t mask_low = 0xff;
	const uint32_t mask_high = 0;
	extern bool _cpu_supports_xsave;
	if (_cpu_supports_xsave) {
		asm volatile ("xsave (%0)" : : "r"(buf), "a"(mask_low), "d"(mask_high) : "%mm0", "%ymm0", "memory");
	} else {
		asm volatile ("fxsave %0" : : "m"(buf) : "memory");
	}
#endif
	Loader * loader = Loader::instance();
	assert(loader != nullptr);

	// It is possible that multiple threads try to access an unresolved function, hence we have to synchronize it
	loader->lookup_sync.write_lock();
	void * r = o.dynamic_resolve(index);
	loader->lookup_sync.write_unlock();
#ifndef NO_FPU
	if (_cpu_supports_xsave) {
		asm volatile ("xrstor (%0)" : : "r"(buf), "a"(mask_low), "d"(mask_high) : "%mm0", "%ymm0", "memory");
	} else {
		asm volatile ("fxrstor %0" : : "m"(buf) : "memory");
	}
#endif
	return r;
}


asm(R"(
.globl _dlresolve
.hidden _dlresolve
.type _dlresolve, @function
.align 16
_dlresolve:
	.cfi_startproc
	# Two arguments (object & index) are passed by PLT
	.cfi_adjust_cfa_offset 16
	endbr64

	# Save base pointer
	push %rbp
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset rbp, 0
	mov %rsp, %rbp

	# Save register
	push %rax
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset rax, 0
	push %rbx
	.cfi_adjust_cfa_offset 8
	.cfi_rel_offset rbx, 0
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

	# Read parameter (pushed bei plt) from stack
	mov 8(%rbp), %rdi
	mov 16(%rbp), %rsi
	# Call high level resolve function
	call __dlresolve
	# Store result (resolved function) on stack (previously used by first parameter pushed by plt)
	mov %rax, 16(%rbp)

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
	pop %rdi
	.cfi_adjust_cfa_offset -8
	pop %rsi
	.cfi_adjust_cfa_offset -8
	pop %rdx
	.cfi_adjust_cfa_offset -8
	pop %rcx
	.cfi_adjust_cfa_offset -8
	pop %rbx
	.cfi_adjust_cfa_offset -8
	pop %rax
	.cfi_adjust_cfa_offset -8
	pop %rbp
	.cfi_adjust_cfa_offset -8

	.cfi_restore rbp
	.cfi_restore rax
	.cfi_restore rbx
	.cfi_restore rcx
	.cfi_restore rdx
	.cfi_restore rsi
	.cfi_restore rdi
	.cfi_restore r8
	.cfi_restore r9
	.cfi_restore r10
	.cfi_restore r11
	.cfi_restore r12
	.cfi_restore r13
	.cfi_restore r14
	.cfi_restore r15

	# Adjust stack (remove second parameter pushed by plt)
	add $8, %rsp
	.cfi_adjust_cfa_offset -8

	# Jump to resolved function
	ret
	.cfi_endproc
)");
