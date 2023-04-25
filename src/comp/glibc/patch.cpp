#include "comp/glibc/patch.hpp"

#include <dlh/log.hpp>
#include <dlh/types.hpp>
#include <dlh/assert.hpp>
#include <dlh/syscall.hpp>

#include "page.hpp"
#include "object/identity.hpp"
#include "comp/glibc/version.hpp"
#include "comp/glibc/libdl/interface.hpp"

namespace GLIBC {

class Patch {
 protected:
	const char * const name;
	bool (*func)(uint8_t*, size_t, uintptr_t);
	uintptr_t arg;

 public:
	Patch(const char * name, bool (*func)(uint8_t*, size_t, uintptr_t), uintptr_t arg)
	  : name(name), func(func), arg(arg) {}

	bool valid() const {
		return name != nullptr;
	}

	bool apply(Object & object, uintptr_t address, size_t size) const {
		if (!valid())
			return false;

		uint8_t * addr = object.compose_pointer(reinterpret_cast<uint8_t *>(object.base + address));

		if (addr == nullptr) {
			LOG_ERROR << "Unable to get compose buffer for " << (void*)address << " for patching " << name << " in " << object << endl;
			return false;
		}

		// Check endbr64
		if (addr[0] == 0xf3 && addr[1] == 0x0f && addr[2] == 0x1e && addr[3] == 0xfa) {
			addr += 4;
			size -= 4;
		}

		// Call patch function
		bool success = func(addr, size, arg);

		if (!success) {
			LOG_ERROR << "Patching '" << name << "' (" << size << " bytes) @ " << (void*)address << " with " << (void*)arg << " in " << object << " failed!"<< endl;
			return false;
		} else {
			LOG_DEBUG << "Patched '" << name << "' (" << size << " bytes) @ " << (void*)address << " with " << (void*)arg << " in " << object << endl;
			return true;
		}
	}
};

class PatchSymbol : public Patch {
	uint32_t hash;
	uint32_t gnuhash;

 public:
	PatchSymbol(const char * name = nullptr, bool (*func)(uint8_t*, size_t, uintptr_t) = nullptr, uintptr_t arg = 0)
	  : Patch(name, func, arg), hash(ELF_Def::hash(name)), gnuhash(ELF_Def::gnuhash(name)) {}


	bool apply(Object & object, const Elf::SymbolTable & symtab) const {
		auto idx = symtab.index(name, hash, gnuhash);
		if (idx == Elf::STN_UNDEF)
			return false;

		auto sym = symtab[idx];
		assert(sym.type() == Elf::STT_FUNC);

		return Patch::apply(object, sym.value(), sym.size());
	}
};


class PatchOffset : public Patch {
	uintptr_t address;
	size_t size;

 public:
	PatchOffset(const char * name = nullptr, uintptr_t address = 0, size_t size = 0, bool (*func)(uint8_t*, size_t, uintptr_t) = nullptr, uintptr_t arg = 0)
	  : Patch(name, func, arg), address(address), size(size) {}

	bool apply(Object & object) const {
		return Patch::apply(object, address, size);
	}
};

static bool redirect_fork_syscall(uint8_t * address, size_t size, uintptr_t arg) {
	// could be done faster, of course.
	for (size_t i = 0; i < size; i++) {
		uint8_t * a = address + i;
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

static bool nops_jmp(uint8_t * address, size_t size, uintptr_t arg) {
	const size_t req = 14;
	if (size < req)
		return false;

	// nop slide
	for (; size > req; size--)
		*(address++) = 0x90;

	// jmp to replace function
	*(address++) = 0xff;
	*(address++) = 0x25;
	*(address++) = 0;
	*(address++) = 0;
	*(address++) = 0;
	*(address++) = 0;
	*(address++) = (arg >> 0) & 0xff;
	*(address++) = (arg >> 8) & 0xff;
	*(address++) = (arg >> 16) & 0xff;
	*(address++) = (arg >> 24) & 0xff;
	*(address++) = (arg >> 32) & 0xff;
	*(address++) = (arg >> 40) & 0xff;
	*(address++) = (arg >> 48) & 0xff;
	*(address++) = (arg >> 56) & 0xff;

	return true;
}


static int _dl_addr_patch(void *address, GLIBC::DL::Info *info, void **mapp, __attribute__((unused)) const uintptr_t **symbolp) {
	return dladdr1(address, info, mapp, GLIBC::DL::RTLD_DL_LINKMAP);
}

extern "C" void _fork_syscall();

// Generic patches for Glibc symbols
static PatchSymbol symbol_fixes[] = {
	{ "_dl_addr",           nops_jmp, reinterpret_cast<uintptr_t>(_dl_addr_patch)},
	{ "__libc_dlopen_mode", nops_jmp, reinterpret_cast<uintptr_t>(dlopen) },
	{ "__libc_dlclose",     nops_jmp, reinterpret_cast<uintptr_t>(dlclose) },
	{ "__libc_dlsym",       nops_jmp, reinterpret_cast<uintptr_t>(dlsym) },
#ifndef COMPATIBILITY_DEBIAN_STRETCH
	{ "__libc_dlvsym",      nops_jmp, reinterpret_cast<uintptr_t>(dlvsym) },
#endif
#if GLIBC_VERSION >= GLIBC_2_34
#define FOKR_SYMNAME "_Fork"
#else
#define FOKR_SYMNAME "__libc_fork"
#endif
	{ FOKR_SYMNAME, redirect_fork_syscall, reinterpret_cast<uintptr_t>(_fork_syscall) },
};

static bool patch_using_symbol_fixes(Object & object) {
	Optional<Elf::SymbolTable> symtab, dynsym;
	for (const auto & section: object.sections)
		if (section.type() == Elf::SHT_DYNSYM)
			dynsym.emplace(section.get_symbol_table());
		else if (section.type() == Elf::SHT_SYMTAB)
			symtab.emplace(section.get_symbol_table());

	if (symtab.has_value() || dynsym.has_value()) {
		bool r = true;
		for (const auto & fix : symbol_fixes)
			r &= (symtab.has_value() && fix.apply(object, symtab.value())) || (dynsym.has_value() && fix.apply(object, dynsym.value()));
		return r;
	} else {
		return false;
	}
}


/* Newer GLIBC (like 2.35 in Ubuntu Jammy and 2.36 in Debian Bookworm) have decided to hide certain GLIBC_PRIVATE symbols.
   This means we need to use offsets gathered from libc debug symbols to be able to intercerpt in those functions.
   The script tools/patch_offsets.sh is able to generate the required offsets...
*/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
static struct {
	uint8_t buildid[20];
	PatchOffset patches[6];
} offset_fixes[] = {
#include "comp/glibc/patch.offsets"
};
#pragma GCC diagnostic pop

static bool patch_using_offset_fixes(Object & object) {
	if (sizeof(offset_fixes) > 0) {
		// This looks quite messy, but it is not as expensive as it looks on a first glimpse. And after all it is only a temporary hack
		for (auto & section: object.sections)
			if (section.type() == Elf::SHT_NOTE)
				for (auto & note : section.get_notes())
					if (note.name() != nullptr && String::compare(note.name(), "GNU") == 0 && note.type() == Elf::NT_GNU_BUILD_ID) {
						for (const auto & offset_fix : offset_fixes)
							if (note.size() == count(offset_fix.buildid) && Memory::compare(offset_fix.buildid, note.description(), note.size()) == 0) {
								bool r = true;
								for (const auto & patch : offset_fix.patches)
									if (patch.valid()) {
										r &= patch.apply(object);
									} else {
										break;
									}
								return r;
							}
						return false;
					}
	}
	return false;
}

bool patch(Object & object) {
	return patch_using_offset_fixes(object) || patch_using_symbol_fixes(object);
}

}  // namespace GLIBC
