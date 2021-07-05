#include "dl.hpp"

#include "object/base.hpp"

extern "C" __attribute__((__used__)) void * dlresolve(const Object & o, size_t index) {
#ifndef NO_FPU
	alignas(64) uint8_t buf[8192];
	asm volatile ("xsave (%0)" : : "r"(buf), "a"(7), "d"(0) : "memory" );
#endif
	auto r = o.dynamic_resolve(index);
#ifndef NO_FPU
	asm volatile ("xrstor (%0)" : : "r"(buf), "a"(7), "d"(0) : "memory" );
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


extern "C" __attribute__ ((visibility("default"))) int dlclose(void *) {
	assert(false);
	return 0;
}

extern "C" __attribute__ ((visibility("default"))) char *dlerror(void) {
	assert(false);
	return nullptr;
}

extern "C" __attribute__ ((visibility("default"))) void *dlopen(const char *, int) {
	assert(false);
	return nullptr;
}
extern "C" __attribute__ ((visibility("default"))) void *dlsym(void *__restrict, const char *__restrict) {
	assert(false);
	return nullptr;
}

/*
TODO:
int    dlclose(void *);
char  *dlerror(void);
void  *dlopen(const char *, int);
void  *dlsym(void *__restrict, const char *__restrict);

typedef struct {
	const char *dli_fname;
	void *dli_fbase;
	const char *dli_sname;
	void *dli_saddr;
} Dl_info;
int dladdr(const void *, Dl_info *);
int dlinfo(void *, int, void *);
*/
