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
.type _dl_resolve, @function
_dl_resolve:
	pop %rdi
	pop %rsi
	call dl_resolve
	jmp *%rax
)");
