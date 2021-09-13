#include "object/base.hpp"

extern "C" __attribute__((__used__)) void * dlresolve(const Object & o, size_t index) {
#ifndef NO_FPU
	const uint32_t mask_low = 0xff;  // assume XSAVE & AVX, TODO: Use CPUID
	const uint32_t mask_high = 0;
	alignas(64) uint8_t buf[4096] = {};
	asm volatile ("xsave (%0)" : : "r"(buf), "a"(mask_low), "d"(mask_high) : "%mm0", "%ymm0", "memory" );
#endif
	auto r = o.dynamic_resolve(index);
#ifndef NO_FPU
	asm volatile ("xrstor (%0)" : : "r"(buf), "a"(mask_low), "d"(mask_high) : "%mm0", "%ymm0", "memory" );
#endif
	return r;
}


asm(R"(
.globl _dlresolve
.hidden _dlresolve
.type _dlresolve, @function
.align 16
_dlresolve:
	# Save base pointer
	push %rbp
	mov %rsp, %rbp

	# Save register
	push %rax
	push %rcx
	push %rdx
	push %rsi
	push %rdi
	push %r8
	push %r9

	# Read parameter (pushed bei plt) from stack
	mov 8(%rbp), %rdi
	mov 16(%rbp), %rsi
	# Call high level resolve function
	call dlresolve
	# Store result (resolved function) in temporary register
	mov %rax, %r11

	# Restore register
	pop %r9
	pop %r8
	pop %rdi
	pop %rsi
	pop %rdx
	pop %rcx
	pop %rax
	pop %rbp

	# Adjust stack (remove parameter pushed by plt)
	add $16, %rsp

	# Jump to resolved function
	jmp *%r11
)");
