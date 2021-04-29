#include "object_dyn.hpp"

#include <elf_rel.hpp>

#include "loader.hpp"
#include "dl.hpp"
#include "generic.hpp"

bool ObjectDynamic::preload() {
	base = file.loader->next_address();
	return preload_segments()
	    && preload_libraries();
}

bool ObjectDynamic::preload_libraries() {
	bool success = true;

	// load needed libaries
	std::vector<const char *> libs, rpath, runpath;
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
		Object * o = file.loader->library(lib, rpath, runpath);
		if (o == nullptr) {
			LOG_WARNING << "Unresolved dependency: " << lib;
			success = false;
		} else {
			dependencies.push_back(o);
		}
	}

	return success;
}

bool ObjectDynamic::run_relocate(bool bind_now) {
	bool success = true;
	if (global_offset_table != 0) {
		auto got = reinterpret_cast<uintptr_t *>(base + global_offset_table);
		// 3 predefined got entries:
		// got[0] is pointer to _DYNAMIC
		got[1] = reinterpret_cast<uintptr_t>(this);
		got[2] = reinterpret_cast<uintptr_t>(_dlresolve);

		// Rest for relocations
		auto got_entries = dynamic_relocations.count() + 3;
		assert(elf.section_by_virt_addr(global_offset_table).entries() == got_entries);
		for (size_t i = 3 ; i < got_entries; i++)
			got[i] += base;

		// Print all entries
		for (size_t i = 0 ; i < got_entries; i++)
			LOG_DEBUG << file.name() << ".got["<< i << "] = " << (void*)got[i];
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

void* ObjectDynamic::dynamic_resolve(size_t index) const {
	auto reloc = dynamic_relocations[index];
	auto need_symbol_index = reloc.symbol_index();
	auto need_symbol_version_index = dynamic_symbols.version(need_symbol_index);
	assert(need_symbol_version_index != Elf::VER_NDX_LOCAL);
	Symbol need_symbol(*this, dynamic_symbols[need_symbol_index], version(need_symbol_version_index));
	LOG_DEBUG << "Need to relocate entry " << index << " with symbol " << need_symbol << "...";
	auto symbol = file.loader->resolve_symbol(need_symbol, file.ns);
	if (symbol) {
		LOG_INFO << "Linking to " << symbol.value() << " in dynamic object " << file.path << "...";
		auto ptr = Relocator(reloc).apply(this->base, symbol.value(), symbol->object.base, this->global_offset_table);
		return reinterpret_cast<void*>(ptr);
	}
	LOG_ERROR << "Unable to resolve relocate entry " << index << " with symbol " << symbol << "...";
	assert(false);
	return nullptr;
}

std::optional<Symbol> ObjectDynamic::resolve_symbol(const Symbol & sym) const {
	auto found = dynamic_symbols.index(sym.name(), sym.hash_value(), sym.gnu_hash_value(), version_index(sym.version));
	if (found != Elf::STN_UNDEF) {
		auto naked_sym = dynamic_symbols[found];
		if (naked_sym.bind() == Elf::STB_GLOBAL && naked_sym.visibility() == Elf::STV_DEFAULT) {
			auto symbol_version_index = dynamic_symbols.version(found);
			return Symbol(*this, naked_sym, version(symbol_version_index));
		}
	}
	return std::nullopt;
}
