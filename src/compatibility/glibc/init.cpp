#include "compatibility/glibc/init.hpp"

#include <elfo/elf.hpp>
#include <dlh/assert.hpp>

#include "compatibility/glibc/version.hpp"
#include "compatibility/glibc/rtld/global.hpp"
#include "compatibility/glibc/libdl/interface.hpp"



namespace GLIBC {
bool patch(const Elf::SymbolTable & symtab, uintptr_t base = 0);

// Guess what: New glibc versions (>= 2.32) call `__libc_early_init` ()
bool init(Loader & loader) {
#if GLIBC_VERSION >= GLIBC_2_32
	if (auto sym = loader.resolve_symbol("__libc_early_init", "GLIBC_PRIVATE")) {
		LOG_DEBUG << "Calling libc early init at " << sym->pointer() << endl;
		assert(sym->type() == Elf::STT_FUNC);
		void (*early_init)(bool) = reinterpret_cast<void (*)(bool)>(sym->pointer());
		early_init(true);
		return true;
	}
	return false;
#else
	(void) loader;
	return true;
#endif
}

// Init shared library structures
bool init(const ObjectDynamic & object) {
	auto & link_map = object.file.glibc_link_map;
	link_map.l_phdr = object.Elf::data(object.header.e_phoff);
	link_map.l_entry = object.header.entry();
	link_map.l_phnum = object.header.e_phnum;
	link_map.l_type = DL::link_map::lt_library;
	link_map.l_relocated = 1;
	link_map.l_init_called = 1;
	link_map.l_global = 1;
#if GLIBC_VERSION >= GLIBC_2_35
	link_map.l_visited = 1;
#endif
	link_map.l_used = 1;
	return true;
}

void init_tls(ObjectIdentity & object, const size_t size, const size_t align, const uintptr_t image, const size_t image_size, const intptr_t offset, size_t modid) {
	object.glibc_link_map.l_tls_blocksize = size;
	object.glibc_link_map.l_tls_align = align;
	object.glibc_link_map.l_tls_initimage = image;
	object.glibc_link_map.l_tls_initimage_size = image_size;
	object.glibc_link_map.l_tls_firstbyte_offset = offset;
	object.glibc_link_map.l_tls_offset = offset;
	object.glibc_link_map.l_tls_modid = modid;
}


}  // namespace GLIBC
