#include "object_dyn.hpp"

#include <elf_rel.hpp>

#include "loader.hpp"
#include "dl.hpp"
#include "generic.hpp"

void * ObjectDynamic::dynamic_resolve(size_t index) const {
	// It is possible that multiple threads try to access an unresolved function, hence we have to use a mutex
	file.loader.mutex.lock();
	auto r = relocate(dynamic_relocations_plt[index]);
	file.loader.mutex.unlock();
	return r;
}

bool ObjectDynamic::preload() {
	base = file.loader.next_address();
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
				rpath.emplace_back(dyn.string());
				break;

			default:
				continue;
		}
	}

	for (auto & lib : libs) {
		auto o = file.loader.library(lib, rpath, runpath);
		if (o == nullptr) {
			LOG_WARNING << "Unresolved dependency: " << lib << endl;
			success = false;
		} else {
			dependencies.push_back(o);
		}
	}

	return success;
}

bool ObjectDynamic::prepare() {
	bool success = true;

	// Perform initial relocations
	for (auto & reloc : dynamic_relocations)
		relocate(reloc);

	// PLT relocations
	if (global_offset_table != 0) {
		auto got = reinterpret_cast<uintptr_t *>(base + global_offset_table);
		// 3 predefined got entries:
		// got[0] is pointer to _DYNAMIC
		got[1] = reinterpret_cast<uintptr_t>(this);
		got[2] = reinterpret_cast<uintptr_t>(_dlresolve);

		// Remainder for relocations
		assert(this->section_by_virt_addr(global_offset_table).entries() >= dynamic_relocations_plt.count() + 3);
		for (const auto & reloc : dynamic_relocations_plt)
			if (file.flags.bind_now)
				relocate(reloc);
			else
				Relocator(reloc).increment_value(base, base);
	}

	return success;
}

void ObjectDynamic::update() {
	for (const auto & relpair : relocations)
		if (!relpair.second.object().is_latest_version())
			relocate(relpair.first);
}

void* ObjectDynamic::relocate(const Elf::Relocation & reloc) const {
	auto need_symbol_index = reloc.symbol_index();
	if (need_symbol_index == 0) {
		auto ptr = Relocator(reloc).fix(this->base, this->global_offset_table);
		return reinterpret_cast<void*>(ptr);
	} else /* TODO: if (!dynamic_symbols.ignored(need_symbol_index)) */ {
		auto need_symbol_version_index = dynamic_symbols.version(need_symbol_index);
		//assert(need_symbol_version_index != Elf::VER_NDX_LOCAL);
		VersionedSymbol need_symbol(dynamic_symbols[need_symbol_index], version(need_symbol_version_index));

		auto symbol = file.loader.resolve_symbol(need_symbol, file.ns);
		if (symbol) {
			LOG_INFO << "Relocating to " << symbol.value() << " in dynamic object " << file.name << "..." << endl;
			relocations.emplace_back(reloc, symbol.value());
			return reinterpret_cast<void*>(Relocator(reloc).fix(this->base, symbol.value(), symbol->object().base, this->global_offset_table));
		} else if (need_symbol.bind() == STB_WEAK) {
			LOG_DEBUG << "Unable to resolve weak symbol " << need_symbol << "..." << endl;
		} else {
			LOG_ERROR << "Unable to resolve symbol " << need_symbol << " for relocation..." << endl;
			assert(false);
		}
	}
	return nullptr;
}

bool ObjectDynamic::patchable() const {
	if (file_previous == nullptr
	 || file_previous->header.identification != this->header.identification
	 || file_previous->header.machine()      != this->header.machine()
	 || file_previous->header.version()      != this->header.version())
		return false;

	assert(file_previous->binary_hash && this->binary_hash);
	LOG_INFO << "Checking if " << this->file.name << " can be patch previous version..." << endl;

	// Check if all required (referenced) symbols to previous object still exist in the new version
	for (const auto & object_file : file.loader.lookup)
		// TODO: If not partial, ignore &object_file == &file
		for (Object * obj = object_file.current; obj != nullptr; obj = obj->file_previous)
			for (const auto & relpair : obj->relocations)
				if (&relpair.second.object() == file_previous) {
					// TODO: Check if relocations are in some protected memory part
					LOG_DEBUG << " - referenced symbol " << relpair.second.name() << endl;
					if (!resolve_symbol(relpair.second)) {
						LOG_WARNING << "Required symbol " << relpair.second.name() << " not found in new version of " << this->file << ") -- not patching the library!" << endl;
						return false;
					}
			}
/*
	// Check if data section has changed (TODO: Other sections?)
	for (const auto &sym : this->binary_hash.value().diff(file_previous->binary_hash.value(), true))
		for (const auto & section : elf.sections)
			if (section.type() == Elf::SHT_PROGBITS && section.allocate() && section.virt_addr() >= sym.address) {
				if (section.writeable() && strcmp(section.name(), ".data") == 0) {
					LOG_WARNING << "Data section changed in new version of " << this->file.name << " -- not patching the library!" << endl;
					return false;
				}
			}
*/
	// All good
	return true;
}

std::optional<VersionedSymbol> ObjectDynamic::resolve_symbol(const VersionedSymbol & sym) const {
	auto found = dynamic_symbols.index(sym.name(), sym.hash_value(), sym.gnu_hash_value(), version_index(sym.version));
	if (found != Elf::STN_UNDEF) {
		auto naked_sym = dynamic_symbols[found];
/*
		// In case we have multiple versions, check if it is mapped here or delegate to previous version (required for partial update)
		if (file.loader.dynamic_update && file_previous != nullptr) {
			bool provided = false;
			for (const auto & seg : memory_map)
				if (seg.target.offset >= naked_sym.value() && seg.target.offset + seg.target.size < naked_sym.value()){
					provided = true;
					break;
				}
			if (!provided)
				return file_previous->resolve_symbol(sym);
		}
*/
		if (naked_sym.bind() == Elf::STB_GLOBAL && naked_sym.visibility() == Elf::STV_DEFAULT) {
			auto symbol_version_index = dynamic_symbols.version(found);
			return VersionedSymbol{naked_sym, version(symbol_version_index)};
		}
	}
	return std::nullopt;
}
