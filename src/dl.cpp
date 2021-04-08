#include "dl.hpp"

#include "object.hpp"
#include "generic.hpp"

extern "C" void * dl_resolve(const Object & o, size_t index) {
	assert(Object::valid(o));
	return o.resolve(index);
}

// TODO: Save registers
asm(R"(
.globl _dl_resolve
.hidden _dl_resolve
.type _dl_resolve, @function
.align 16
_dl_resolve:
	# Save base pointer
	push %rbx
	mov %rsp, %rbx

	# Save register
	push %rax
	push %rcx
	push %rdx
	push %rsi
	push %rdi
	push %r8
	push %r9

	# Read parameter (pushed bei plt) from stack
	mov 8(%rbx), %rdi
	mov 16(%rbx), %rsi
	# Call high level resolve function
	call dl_resolve
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

	# Adjust stack (remove %rbx and parameter pushed by plt)
	add $24, %rsp

	# Jump to resolved function
	jmp *%r11
)");
