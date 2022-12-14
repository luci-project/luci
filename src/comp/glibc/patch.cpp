#include "comp/glibc/patch.hpp"

#include <dlh/log.hpp>
#include <dlh/types.hpp>
#include <dlh/assert.hpp>
#include <dlh/syscall.hpp>

#include "page.hpp"
#include "comp/glibc/version.hpp"
#include "comp/glibc/libdl/interface.hpp"

namespace GLIBC {

class Patch {
	const char * symname;
	uint32_t hash;
	uint32_t gnuhash;
	bool (*func)(uint8_t*, size_t, uintptr_t);
	uintptr_t arg;

 public:
	Patch(const char * symname, bool (*func)(uint8_t*, size_t, uintptr_t), uintptr_t arg)
	  : symname(symname), hash(ELF_Def::hash(symname)), gnuhash(ELF_Def::gnuhash(symname)), func(func), arg(arg) {}

	bool apply(const Elf::SymbolTable & symtab, uintptr_t base = 0) const {
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
			LOG_ERROR << "Unprotecting " << page_size << " Bytes at " << page_start << " for patching " << symname << " failed: " << protect.error_message() << endl;
			return false;
		}

		uint8_t * addr = reinterpret_cast<uint8_t *>(start);

		// Check endbr64
		if (addr[0] == 0xf3 && addr[1] == 0x0f && addr[2] == 0x1e && addr[3] == 0xfa) {
			addr += 4;
			size -= 4;
		}

		// Call patch function
		bool success = func(addr, size, arg);

		//assert(reinterpret_cast<uintptr_t>(addr - 1) == start + size);
		auto reprotect = Syscall::mprotect(page_start, page_size, PROT_READ | PROT_EXEC);
		if (!reprotect.success()) {
			LOG_ERROR << "Reprotecting " << page_size << " Bytes at " << page_start << " for patching " << symname << " failed: " << reprotect.error_message() << endl;
			return false;
		} else if (!success) {
			LOG_ERROR << "Patching '" << symname << "' @ " << (void*)start << " with " << (void*)arg << " failed!"<< endl;
			return false;
		} else {
			LOG_DEBUG << "Patched '" << symname << "' @ " << (void*)start << " with " << (void*)arg << endl;
			return true;
		}
	}
};

static bool redirect_fork_syscall(uint8_t * addr, size_t size, uintptr_t arg) {
	// could be done faster, of course.
	for (size_t i = 0; i < size; i++) {
		uint8_t * a = addr + i;
		// Find fork syscall instructions
		if (
		    a[0] == 0xbf && a[1] == 0x11 && a[2] == 0x00 && a[3] == 0x20 && a[4] == 0x01 &&                                    // mov    $0x1200011,%edi
#if GLIBC_VERSION >= GLIBC_2_34
		    a[5] == 0x4c && a[6] == 0x8d && a[7] == 0x90 && a[8] == 0xd0 && a[9] == 0x02 && a[10] == 0x00 && a[11] == 0x00 &&  // lea    0x2d0(%rax),%r10
		    a[12] == 0xb8 && a[13] == 0x38 && a[14] == 0x00 && a[15] == 0x00 && a[16] == 0x00 &&                               // mov    $0x38,%eax
			a[17] == 0x0f && a[18] == 0x05                                                                                     // syscall
#else
		    a[5] == 0xb8 && a[6] == 0x38 && a[7] == 0x00 && a[8] == 0x00 && a[9] == 0x00 &&                                    // mov    $0x38,%eax
		    a[10] == 0x0f && a[11] == 0x05                                                                                     // syscall
#endif
		 ){

			// Replace with call to replacement function
			// movabs $arg, %rax
			*(a++) = 0x48;
			*(a++) = 0xb8;
			*(a++) = (arg >> 0) & 0xff;
			*(a++) = (arg >> 8) & 0xff;
			*(a++) = (arg >> 16) & 0xff;
			*(a++) = (arg >> 24) & 0xff;
			*(a++) = (arg >> 32) & 0xff;
			*(a++) = (arg >> 40) & 0xff;
			*(a++) = (arg >> 48) & 0xff;
			*(a++) = (arg >> 56) & 0xff;
			// callq *%rax
			*(a++) = 0xff;
			*(a++) = 0xd0;
#if GLIBC_VERSION >= GLIBC_2_34
			// nop (7 byte)
			*(a++) = 0x0f;
			*(a++) = 0x1f;
			*(a++) = 0x80;
			*(a++) = 0x00;
			*(a++) = 0x00;
			*(a++) = 0x00;
			*(a++) = 0x00;
#endif
			return true;
		}
	}
	return false;
}

static bool nops_jmp(uint8_t * addr, size_t size, uintptr_t arg) {
	const size_t req = 14;
	if (size < req)
		return false;

	// nop slide
	for (; size > req; size--)
		*(addr++) = 0x90;

	// jmp to replace function
	*(addr++) = 0xff;
	*(addr++) = 0x25;
	*(addr++) = 0;
	*(addr++) = 0;
	*(addr++) = 0;
	*(addr++) = 0;
	*(addr++) = (arg >> 0) & 0xff;
	*(addr++) = (arg >> 8) & 0xff;
	*(addr++) = (arg >> 16) & 0xff;
	*(addr++) = (arg >> 24) & 0xff;
	*(addr++) = (arg >> 32) & 0xff;
	*(addr++) = (arg >> 40) & 0xff;
	*(addr++) = (arg >> 48) & 0xff;
	*(addr++) = (arg >> 56) & 0xff;

	return true;
}


static int _dl_addr_patch(void *address, GLIBC::DL::Info *info, void **mapp, __attribute__((unused)) const uintptr_t **symbolp) {
	return dladdr1(address, info, mapp, GLIBC::DL::RTLD_DL_LINKMAP);
}

extern "C" void _fork_syscall();

static Patch fixes[] = {
	{ "_dl_addr", nops_jmp, reinterpret_cast<uintptr_t>(_dl_addr_patch)},
	{ "__libc_dlopen_mode", nops_jmp, reinterpret_cast<uintptr_t>(dlopen) },
	{ "__libc_dlclose", nops_jmp, reinterpret_cast<uintptr_t>(dlclose) },
	{ "__libc_dlsym", nops_jmp, reinterpret_cast<uintptr_t>(dlsym) },
	{ "__libc_dlvsym", nops_jmp, reinterpret_cast<uintptr_t>(dlvsym) },
#if GLIBC_VERSION >= GLIBC_2_34
	{ "_Fork", redirect_fork_syscall, reinterpret_cast<uintptr_t>(_fork_syscall) },
#else
	{ "__libc_fork", redirect_fork_syscall, reinterpret_cast<uintptr_t>(_fork_syscall) },
#endif
};

bool patch(const Elf::SymbolTable & symtab, uintptr_t base) {
	bool r = false;
	for (const auto & fix : fixes)
		if (fix.apply(symtab, base))
			r = true;
	return r;
}

}  // namespace GLIBC
