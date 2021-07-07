#include "compatibility/dl.hpp"

#include <dlh/utils/log.hpp>

#include "compatibility/export.hpp"
#include "object/base.hpp"
#include "loader.hpp"

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

Loader * loader = nullptr;  // Temp. fix, defined in main()

EXPORT int dlclose(void *) {
	assert(false);
	return 0;
}

EXPORT char *dlerror(void) {
	assert(false);
	return nullptr;
}

EXPORT void *dlopen(const char *, int) {
	assert(false);
	return nullptr;
}

EXPORT int dladdr1(void *addr, DL::Info *info, void **extra_info, int flags) {
	LOG_DEBUG << "dladdr: resolving " << addr << "..." << endl;
	auto loader = Loader::instance();
	assert(loader != nullptr);

	auto o = loader->resolve_object(reinterpret_cast<uintptr_t>(addr));
	if (o == nullptr)
		return 0;

	LOG_DEBUG << "dladdr: " << addr << " is part of " << *o << endl;
	assert(info != nullptr);
	info->dli_fname = o->file.filename;
	info->dli_fbase = o->base;
	if (flags == DL::RTLD_DL_LINKMAP)
		*extra_info = reinterpret_cast<void*>(&(o->file));
	auto sym = o->resolve_symbol(reinterpret_cast<uintptr_t>(addr));
	if (sym) {
		LOG_DEBUG << "dladdr: " << addr << " belongs to " << sym << endl;
		info->dli_sname = sym->name();
		info->dli_saddr = o->base + sym->value();
		if (flags == DL::RTLD_DL_SYMENT)
			*extra_info = (void *)(sym->_data);
	} else {
		info->dli_sname = nullptr;
		info->dli_saddr = 0;
	}

	return 1;
}

EXPORT int dladdr(void *addr, DL::Info *info) {
	return dladdr1(addr, info, 0, 0);
}

EXPORT void *dlsym(void *__restrict, const char *__restrict) {
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
