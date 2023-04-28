// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "comp/glibc/start.hpp"

#include <dlh/syscall.hpp>

extern "C" void * ptr_main;
extern "C" void * ptr_libc_start_main;

extern "C" void _libc_start();
extern "C" void _luci_start();
extern "C" void _luci_exit(int value);

void * ptr_main = nullptr;
void * ptr_libc_start_main = nullptr;

void _luci_exit(int value) {
	LOG_INFO << "`main` finished with exit code " << value << endl;
	Syscall::exit(value);
}

namespace GLIBC {
static void * resolve_symbol(Loader & loader, const char *name) {
	if (auto sym = loader.resolve_symbol(name, nullptr, NAMESPACE_BASE, loader.self, Loader::RESOLVE_EXCEPT_OBJECT)) {
		if (sym->type() == Elf::STT_FUNC || sym->type() == Elf::STT_GNU_IFUNC) {
			LOG_DEBUG << "Found `" << name << "` symbol in " << sym->object() << endl;
			return sym->pointer();
		} else {
			LOG_WARNING << "Found `" << name << "` symbol in " << sym->object() << ", but it is not a function type!" << endl;
		}
	}
	return nullptr;
}
void * start_entry(Loader & loader) {
	void * symptr = nullptr;
	if ((symptr = resolve_symbol(loader, "_start")) != nullptr) {
		return symptr;
	} else if ((symptr = resolve_symbol(loader, "main")) != nullptr) {
		ptr_main = symptr;
		if (ptr_libc_start_main == nullptr && (ptr_libc_start_main = resolve_symbol(loader, "__libc_start_main")) == nullptr) {
			LOG_WARNING << "No `__libc_start_main` found -- using luci dummy to execute `main`" << endl;
			return reinterpret_cast<void *>(_luci_start);
		} else {
			return reinterpret_cast<void *>(_libc_start);
		}
	} else {
		LOG_ERROR << "Neither `_start` nor `main` found!" << endl;
		return nullptr;
	}
}
}  // namespace GLIBC

/*__libc_start_main requires
	main:		%rdi
	argc:		%rsi
	argv:		%rdx
	init:		%rcx
	fini:		%r8
	rtld_fini:	%r9
	stack_end:	stack.

according to sysdeps/x86_64/start.S
*/
asm(R"(
.globl _libc_start
.hidden _libc_start
.type _libc_start, @function
.align 16
_libc_start:
	.cfi_startproc
	.cfi_undefined rip
	endbr64

	# Clear the frame pointer (for debugging, outermost frame)
	xor %ebp,%ebp

	# rtld_fini argument
	mov %rdx, %r9

	# argc
	pop %rsi
	# argv
	mov %rsp,%rdx

	# align stack pointer
	and $-16,%rsp
	# (garbage for alignment)
	push %rax
	# (provide stack address)
	push %rsp

	# We don't have/support init and fini -> zero out
	xorl %ecx,%ecx
	xorl %r8d,%r8d

	# Set main
	mov ptr_main@GOTPCREL(%rip),%r12
	mov (%r12),%rdi

	# call __libc_start_main
	mov ptr_libc_start_main@GOTPCREL(%rip),%r12
	call *(%r12)

	# We shall never return. However,...
	hlt
	.cfi_endproc

.globl _luci_start
.hidden _luci_start
.type _luci_start, @function
.align 16
_luci_start:
	.cfi_startproc
	endbr64

	.cfi_undefined rip

	# Clear the frame pointer (for debugging, outermost frame)
	xor %ebp,%ebp

	# argc
	pop %rdi
	# argv
	mov %rsp,%rsi

	# align stack pointer
	and $-16,%rsp

	# call main
	mov ptr_main@GOTPCREL(%rip),%r12
	call *(%r12)

	# return value as new argument
	mov %rax,%rdi

	# Call exit wrapper
	call _luci_exit
	.cfi_endproc
)");
