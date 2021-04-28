#include "dl.hpp"

#include "object.hpp"
#include "generic.hpp"

extern "C" __attribute__((__used__)) void * dlresolve(const Object & o, size_t index) {
	alignas(64) uint8_t buf[1024];
	asm volatile ("xsave (%0)" : : "r"(buf), "a"(7), "d"(0) : "memory" );
	auto r = o.dynamic_resolve(index);
	asm volatile ("xrstor (%0)" : : "r"(buf), "a"(7), "d"(0) : "memory" );
	return r;
}


asm(R"(
.globl _dlresolve
.hidden _dlresolve
.type _dlresolve, @function
.align 16
_dlresolve:
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

	# Adjust stack (remove %rbx and parameter pushed by plt)
	add $24, %rsp

	# Jump to resolved function
	jmp *%r11
)");


extern "C" int dlclose(void *) {
	return 0;
}

extern "C" char *dlerror(void) {
	return nullptr;
}

extern "C" void *dlopen(const char *, int) {
	return nullptr;
}
extern "C" void *dlsym(void *__restrict, const char *__restrict) {
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
