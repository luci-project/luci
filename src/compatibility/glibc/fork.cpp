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

	auto loader = Loader::instance();
	assert(loader != nullptr);

	// Prevent modifications during fork
	loader->lookup_sync.read_lock();

	HashMap<int,int> replace_fd;
	if (loader->config.dynamic_update) {
		for (auto & i: loader->lookup)
			for (auto o = i.current; o != nullptr; o = o->file_previous)
				for (auto & m : o->memory_map) {
					int old_fd = m.target.fd;
					if (old_fd != -1 && !replace_fd.contains(old_fd)) {
						int new_fd = m.shmemdup();
						assert(new_fd != -1);
						replace_fd.insert(old_fd, new_fd);
						LOG_DEBUG << "Fork created copy of shared memory at " << (void*) m.target.address() << " (fd " << old_fd << " -> " << new_fd << ")" << endl;
					}
				}
		LOG_INFO << "Fork needs to replace " << replace_fd.size() << " shared memory files" << endl;
	}

	pid_t child = 0;
	int r = -1;
	if (auto clone = Syscall::clone(CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID | SIGCHLD, 0, NULL, &child, 0)) {
		if (clone.success() && (r = clone.value()) == 0) {
			// Remap
			int remaps = 0;
			for (auto & i: loader->lookup)
				for (auto o = i.current; o != nullptr; o = o->file_previous)
					for (auto & m : o->memory_map) {
						int old_fd = m.target.fd;
						if (old_fd != -1) {
							int new_fd = replace_fd[old_fd];
							LOG_DEBUG << "Fork child remapping shared memory at " << (void*) m.target.address() << " (fd " << old_fd << " -> " << new_fd << ")" << endl;
							m.unmap();
							assert(new_fd != -1);
							m.target.fd = new_fd;
							m.map();
							m.protect();
							remaps++;
						}
					}
			// Close (parent) shared memory file descriptor
			for (auto & f : replace_fd)
				Syscall::close(f.key);
			// Set own Thread ID
			Thread::self()->tid = child;
			// Start handler threads
			loader->start_handler_threads();
		} else if (loader->config.dynamic_update) {
			// Close (childs) shared memory file descriptors
			for (auto & f : replace_fd)
				Syscall::close(f.value);
		}
	}
	loader->lookup_sync.read_unlock();

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
