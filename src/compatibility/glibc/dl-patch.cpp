#include "compatibility/glibc/dl-patch.hpp"

#include <dlh/types.hpp>
#include "compatibility/glibc/libdl/interface.hpp"
#include "compatibility/patch.hpp"

namespace GLIBC {
namespace DL {

static int _dl_addr_patch(void *address, GLIBC::DL::Info *info, void **mapp, __attribute__((unused)) const uintptr_t **symbolp) {
	return dladdr1(address, info, mapp, GLIBC::DL::RTLD_DL_LINKMAP);
}

static Patch fixes[] = {
	{ "_dl_addr", reinterpret_cast<uintptr_t>(_dl_addr_patch) },
	{ "__libc_dlopen_mode", reinterpret_cast<uintptr_t>(dlopen) },
	{ "__libc_dlclose", reinterpret_cast<uintptr_t>(dlclose) },
	{ "__libc_dlsym", reinterpret_cast<uintptr_t>(dlsym) },
	{ "__libc_dlvsym", reinterpret_cast<uintptr_t>(dlvsym) }
};

bool patch(const Elf::SymbolTable & symtab, uintptr_t base) {
	bool r = false;
	for (const auto & fix : fixes)
		if (fix.apply(symtab, base))
			r = true;
	return r;
}

}  // namespace DL
}  // namespace GLIBC
