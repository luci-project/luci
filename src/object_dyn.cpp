#include "object_dyn.hpp"

#include "generic.hpp"

bool ObjectDynamic::preload() {
	return preload_segments(next_address())
	    && preload_libraries();
}

bool ObjectDynamic::preload_libraries() {
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
			case Elf::DT_JMPREL:
			case Elf::DT_REL:
			case Elf::DT_RELA:
				relocation_tables.push_back(ELFIO::const_relocation_section_accessor(elf, get_section(dyn.value())));
				assert(relocation_tables.size() == 1);
				break;
*/
			case Elf::DT_PLTGOT:
				global_offset_table = dyn.value();
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

	bool success = true;
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


Symbol ObjectDynamic::symbol(const Symbol & sym) const {
	bool version_weak = false;
	auto version = version_index(sym.version.name, sym.version.hash, version_weak);
	auto found = dynamic_symbols.index(sym.name(), version);
	if (found != Elf::STN_UNDEF) {
		return Symbol(*this, dynamic_symbols[found], version_name(dynamic_symbols.version(found)), version_weak);
	} else {
		return Symbol(*this);
	}
}

bool ObjectDynamic::relocate() {
	bool success = true;
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
