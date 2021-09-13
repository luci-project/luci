#include "compatibility/patch.hpp"

#include <dlh/log.hpp>
#include <dlh/types.hpp>
#include <dlh/assert.hpp>
#include <dlh/syscall.hpp>

#include "page.hpp"

bool Patch::apply(const Elf::SymbolTable & symtab, uintptr_t base) const {
	auto idx = symtab.index(symname, hash, gnuhash);
	if (idx == Elf::STN_UNDEF)
		return false;

	auto sym = symtab[idx];
	assert(sym.type() == Elf::STT_FUNC);

	size_t size = sym.size();
	uintptr_t start = base + sym.value();

	unsigned long page_start = start - (start % Page::SIZE);
	unsigned long page_size = (start - page_start) + size;

	if (auto protect = Syscall::mprotect(page_start, page_size, PROT_READ | PROT_EXEC | PROT_WRITE); protect.failed()) {
		LOG_ERROR << "Unprotecting " << page_size << " Bytes at " << page_start << " failed: " << protect.error_message() << endl;
		return false;
	}

	uint8_t * addr = reinterpret_cast<uint8_t *>(start);
	size_t req = 14;

	// Check endbr64
	if (addr[0] == 0xf3 && addr[1] == 0x0f && addr[2] == 0x1e && addr[3] == 0xfa) {
		addr += 4;
		size -= 4;
	}
	assert(size > 10);

	// nop slide
	for (; size > req; size--)
		*(addr++) = 0x90;

	// jmp
	*(addr++) = 0xff;
	*(addr++) = 0x25;
	*(addr++) = 0;
	*(addr++) = 0;
	*(addr++) = 0;
	*(addr++) = 0;
	*(addr++) = (replace >> 0) & 0xff;
	*(addr++) = (replace >> 8) & 0xff;
	*(addr++) = (replace >> 16) & 0xff;
	*(addr++) = (replace >> 24) & 0xff;
	*(addr++) = (replace >> 32) & 0xff;
	*(addr++) = (replace >> 40) & 0xff;
	*(addr++) = (replace >> 48) & 0xff;
	*(addr++) = (replace >> 56) & 0xff;

	//assert(reinterpret_cast<uintptr_t>(addr - 1) == start + size);
	auto reprotect = Syscall::mprotect(page_start, page_size, PROT_READ | PROT_EXEC);
	if (!reprotect.success()) {
		LOG_ERROR << "Reprotecting " << page_size << " Bytes at " << page_start << " failed: " << reprotect.error_message() << endl;
		return false;
	}

	LOG_DEBUG << "Replaced '" << symname << "' @ " << (void*)start << " with jump to " << (void*)replace << endl;
	return true;
}
