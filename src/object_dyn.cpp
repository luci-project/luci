#include "object_dyn.hpp"

#include <elf_rel.hpp>

#include "dl.hpp"
#include "generic.hpp"

bool ObjectDynamic::preload() {
	base = next_address();
	return preload_segments()
	    && preload_libraries();
}

bool ObjectDynamic::preload_libraries() {
	bool success = true;

	// load needed libaries
	std::vector<std::string> libs, rpath, runpath;
	for (auto &dyn: dynamic) {
		switch (dyn.tag()) {
			case Elf::DT_NEEDED:
				libs.emplace_back(dyn.string());
				break;
			case Elf::DT_RPATH:
				rpath.emplace_back(dyn.string());
				break;
			case Elf::DT_RUNPATH:
				runpath.emplace_back(dyn.string());
				break;

/*
			case Elf::DT_REL:
			case Elf::DT_RELA:
				relocation_tables.push_back(ELFIO::const_relocation_section_accessor(elf, get_section(dyn.value())));
				assert(relocation_tables.size() == 1);
				break;
*/
			case Elf::DT_PLTGOT:
				global_offset_table = dyn.value();
				LOG_DEBUG << "GOT of " << (void*)this << " is " << global_offset_table;
				break;

			case Elf::DT_INIT:
				init.func = reinterpret_cast<void(*)()>(dyn.value());
				break;

			case Elf::DT_FINI:
				fini.func = reinterpret_cast<void(*)()>(dyn.value());
				break;

			case Elf::DT_PREINIT_ARRAY:
				init.func_prearray = reinterpret_cast<void (**)()>(dyn.value());
				break;

			case Elf::DT_PREINIT_ARRAYSZ:
				init.func_prearray_size = reinterpret_cast<size_t>(dyn.value());
				break;

			case Elf::DT_INIT_ARRAY:
				init.func_array = reinterpret_cast<void (**)()>(dyn.value());
				break;

			case Elf::DT_INIT_ARRAYSZ:
				init.func_array_size = reinterpret_cast<size_t>(dyn.value());
				break;

			case Elf::DT_FINI_ARRAY:
				fini.func_array = reinterpret_cast<void (**)()>(dyn.value());
				break;

			case Elf::DT_FINI_ARRAYSZ:
				fini.func_array_size = reinterpret_cast<size_t>(dyn.value());
				break;

			default:
				continue;
		}
	}


	for (auto & lib : libs) {
		Object * o = load_library(lib, rpath, runpath);
		if (o == nullptr) {
			LOG_WARNING << "Unresolved dependency: " << lib;
			success = false;
		} else {
			dependencies.push_back(o);
		}
	}

	return success;
}

void* ObjectDynamic::resolve(size_t index) const {
	auto reloc = dynamic_relocations[index];
	auto need_symbol_index = reloc.symbol_index();
	auto need_symbol_version_index = dynamic_symbols.version(need_symbol_index);
	assert(need_symbol_version_index != Elf::VER_NDX_LOCAL);
	auto need_symbol = Symbol(*this, dynamic_symbols[need_symbol_index], version(need_symbol_version_index));
	LOG_DEBUG << "Need to relocate entry " << index << " with symbol " << need_symbol << "...";
	Symbol res = find_symbol(need_symbol);
	if (res.valid()) {
		LOG_INFO << "Linking to " << res << " in dynamic object " << path << "...";
		auto ptr = Relocator(reloc).apply(this->base, res, res.object.base, this->global_offset_table);
		return reinterpret_cast<void*>(ptr);
	}
	LOG_ERROR << "Unable to resolve relocate entry " << index << " with symbol " << need_symbol << "...";
	assert(false);
	return nullptr;
}

Symbol ObjectDynamic::symbol(const Symbol & sym) const {
	auto found = dynamic_symbols.index(sym.name(), version_index(sym.version));
	if (found != Elf::STN_UNDEF) {
		auto symbol_version_index = dynamic_symbols.version(found);
		return Symbol(*this, dynamic_symbols[found], version(symbol_version_index));
	} else {
		return Symbol(*this);
	}
}

bool ObjectDynamic::relocate() {
	bool success = true;
	if (global_offset_table != 0) {
		auto got = reinterpret_cast<uintptr_t *>(base + global_offset_table);
		// 3 predefined got entries:
		// got[0] is pointer to _DYNAMIC
		got[1] = reinterpret_cast<uintptr_t>(this);
		got[2] = reinterpret_cast<uintptr_t>(_dl_resolve);

		// Rest for relocations
		auto got_entries = dynamic_relocations.count() + 3;
		assert(elf.section_by_virt_addr(global_offset_table).entries() == got_entries);
		for (size_t i = 3 ; i < got_entries; i++)
			got[i] += base;

		// Print all entries
		for (size_t i = 0 ; i < got_entries; i++)
			LOG_DEBUG << file_name() << ".got["<< i << "] = " << (void*)got[i];
	}
	/*
	// Load unresolved symbols
	for (auto & table : symbol_tables) {
		ELFIO::Elf_Xword sym_no = table.get_symbols_num();
		for (ELFIO::Elf_Xword i = 0; i < sym_no; ++i) {
			Symbol sym(table, i);
			if (sym.is_valid() && !sym.is_defined()) {
				auto res = find_symbol(sym.name);
				if (res.first != nullptr && res.second.type == sym.type) {
					LOG_INFO << "Found " << res.second.name << " in " << res.first->path;
					// TODO: Reloc
				} else {
					LOG_ERROR << "Cannot find " << sym.name << " for " << path;
					success = false;
				}
			}
		}
	}*/
	return success;
}
