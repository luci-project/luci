#include "object/relocatable.hpp"

#include <elfo/elf_rel.hpp>
#include <dlh/log.hpp>
#include <dlh/string.hpp>
#include <dlh/auxiliary.hpp>

#include "comp/glibc/start.hpp"
#include "loader.hpp"

/* Files will be modified in memory!
 * Create GOT/PLT (with custom on-the-fly relocations for resolving) unless static -- (perhaps!) additional relocs
 * Separate text / [ro]data per object
 * calc global BSS section size
 * 2 GB wg RIP adressierung
 * multible rellocs
 * TODO Linker symbols: etext edata end
 */

uintptr_t ObjectRelocatable::offset = 0;
//uintptr_t ObjectRelocatable::base = 0x300000000000UL;

static uintptr_t freeaddr = 0x300000000000UL;  // TODO: replace by (extended) next_address

ObjectRelocatable::ObjectRelocatable(ObjectIdentity & file, const Object::Data & data)
  : Object{file, data},
    symbols{this->symbol_table()} {
	base = freeaddr; // TODO!
}

bool ObjectRelocatable::preload() {
	// TODO flags.immutable_source  (should always be set, right?)

	for (auto & section : this->sections) {
		if (section.size() == 0)
			continue;

		switch (section.type()) {
			case SHT_NOBITS:
			case SHT_PROGBITS:
			case SHT_INIT_ARRAY:
			case SHT_FINI_ARRAY:
			case SHT_PREINIT_ARRAY:
				if (section.allocate()) {
					// (writeable) data
					if (section.writeable() && this->file_previous != nullptr) {
						// TODO: Fix symbol table [offsets] to point to file_previous' symbols

						// FEATURE: New symbols should be possible (in theory)
					} else {
						size_t additional_size = 0;
						if (strcmp(section.name(), ".init") == 0 || strcmp(section.name(), ".fini") == 0) {
							// The init/fini section consists only from calls.
							// They need to be fixed after mapping
							init_sections.push_back(section);
							// We need an additional byte for the return instruction
							additional_size = 1;
						}
						// Mapping
						auto mem = memory_map.emplace_back(*this, section, offset, additional_size, base);

						// Fixup section entry,
						assert(section.virt_addr() == 0);
						auto data = const_cast<Elf::Shdr *>(section.ptr());
						data->sh_addr = offset;

						// Fixup symbols & relocations
						adjust_offsets(this->sections.index(section), offset);

						// increase offset to next free page
						offset += mem->target.page_size();
					}
				}
				break;

			case SHT_REL:
			case SHT_RELA:
				relocation_tables.push_back(section.get_relocations());
				break;


		}
	}

	// Allocate global bss
	if (this->file_previous == nullptr) {
		size_t global_bss = 0;
		// Check symbol table for uninitialized block entries
		for (const auto & sym : symbols)
			if (sym.size() != 0 && sym.section_index() == Elf::SHN_COMMON) {
				// Value is alignment
				auto start = Math::align_up(global_bss, sym.value());
				// Add to global bss size
				global_bss = start + sym.size();
				// This is only allowed since we have a COW mapping
				auto data = const_cast<Elf::Sym *>(sym.ptr());
				// Fixup value (= offset)
				data->st_value = offset + start;
			}
		// Allocate
		if (global_bss > 0) {
			auto mem = memory_map.emplace_back(*this, offset, global_bss, base);
			// increase offset to next free page
			offset += mem->target.page_size();
		}
	}

	freeaddr = Math::align_up(base + offset + 0x10000, Page::SIZE);

	return memory_map.size() > 0;
}


bool ObjectRelocatable::fix() {
	// TODO: add _start routine etc
	return true;

}

void ObjectRelocatable::adjust_offsets(int16_t index, uintptr_t offset) {
	// Symbol table
	for (auto sym : symbols)
		if (sym.section_index() == index && (sym.type() == STT_OBJECT || sym.type() == STT_FUNC || sym.type() == STT_SECTION || sym.type() == STT_GNU_IFUNC)) {
			// This is only allowed since we have a COW mapping
			auto data = const_cast<Elf::Sym *>(sym.ptr());
			// Fixup value (= offset)
			data->st_value += offset;
			LOG_INFO << sym.name() << " @ " << (void*)sym.value() << endl;
	}

	// Relocations
	for (auto & linked : this->sections) {
		if (linked.size() == 0)
			continue;

		switch (linked.type()) {
			case SHT_REL:
				// Fixup relocations to this section
				if (linked.info_link() && static_cast<int16_t>(linked.info()) == index)
					for (auto rel : linked.get_array<RelocationWithoutAddend>()) {
						// This is only allowed since we have a COW mapping
						auto data = const_cast<Elf::Rel *>(rel.ptr());
						// Fixup offset
						data->r_offset += offset;
					}
				break;

			case SHT_RELA:
				// Fixup relocations to this section
				if (linked.info_link() && static_cast<int16_t>(linked.info()) == index)
					for (auto rel : linked.get_array<RelocationWithAddend>()) {
						// This is only allowed since we have a COW mapping
						auto data = const_cast<Elf::Rela *>(rel.ptr());
						// Fixup offset
						data->r_offset += offset;
					}
				break;
		}
	}
}


bool ObjectRelocatable::prepare() {
	LOG_INFO << "Prepare " << *this << " with " << (void*)global_offset_table << endl;
	bool success = true;

	// Perform initial relocations
	for (auto & relocations : relocation_tables)
		for (auto & reloc : relocations) {
			//LOG_INFO << "Relocate " << (void*)reloc.offset() << " = " << (void*)reloc.symbol().address() << endl;
			if (relocate(reloc) == nullptr)
				success = false;
		}

	// Add ret statement to section which need fixing
	for (auto & section : this->init_sections) {
		assert(section.size() > 0 && section.type() == SHT_PROGBITS);
		char * end = reinterpret_cast<char *>(base + section.virt_addr() + section.size());
		LOG_INFO << "Add retq instruction to " << (void*)end << " at " << section.name() << endl;
		*end = 0xc3;  // retq
	}

	for (const auto & m : memory_map)
		m.dump();

	return success;
}


bool ObjectRelocatable::update() {
	// Adjust relocations:
	// - Fix all relocations to STT_FUNC
	// - Load RODATA STT_OBJECTs?
	return true;
}


bool ObjectRelocatable::patchable() const {
	// check if updateable
	return true;
}


Optional<VersionedSymbol> ObjectRelocatable::resolve_symbol(const char * name, uint32_t hash, uint32_t gnu_hash, const VersionedSymbol::Version & version) const {
	(void) hash;
	(void) gnu_hash;
	(void) version;

	// TODO: Use hash map with global symbol names for faster lookup.
	for (const auto & sym : symbols)
		if (sym.section_index() != Elf::SHN_UNDEF && sym.bind() != Elf::STB_LOCAL && sym.visibility() == Elf::STV_DEFAULT && String::compare(name, sym.name()) == 0) {
			VersionedSymbol vs{ sym };
			return { vs };
		}

	return {};
}


Optional<VersionedSymbol> ObjectRelocatable::resolve_symbol(uintptr_t addr) const {
	if (addr > base) {
		uintptr_t offset = addr - base;
		for (const auto & sym : symbols)
			if (sym.section_index() != Elf::SHN_UNDEF && offset >= sym.value() && offset <= sym.value() + sym.size()) {
				VersionedSymbol vs{sym};
				return { vs };
			}
	}
	return {};
}

void* ObjectRelocatable::relocate(const Elf::Relocation & reloc) const {
	// Initialize relocator object
	Relocator relocator(reloc, 0);
	// find symbol
	if (reloc.symbol_index() == 0) {
		// Local relocation (without symbol) -- not sure if can occure at all
		LOG_INFO << "Relocating local in " << *this <<  endl;
		return reinterpret_cast<void*>(relocator.fix_internal(this->base, 0, this->file.tls_module_id, this->file.tls_offset));
	} else {
		const auto needed_symbol = reloc.symbol();
		// We omit the PLT and directly link the target, hence replacing `plt_entry` with `base + symbol.value()``
		auto plt_entry = base + needed_symbol.value();
		if (needed_symbol.section_index() != Elf::SHN_UNDEF) {
			// Local symbol
			//relocations.insert(reloc, needed_symbol.value());
			LOG_INFO << "Relocating local symbol in " << *this << " with " << needed_symbol.name() << " at " << (void*)needed_symbol.value()  << endl;
			return reinterpret_cast<void*>(relocator.fix_internal(this->base, plt_entry, this->file.tls_module_id, this->file.tls_offset));
		} else {
			// external symbol
			// COPY Relocations have a defined symbol with the same name
			Loader::ResolveSymbolMode mode = relocator.is_copy() ? Loader::RESOLVE_EXCEPT_OBJECT : (file.flags.bind_deep == 1 ? Loader::RESOLVE_OBJECT_FIRST : Loader::RESOLVE_DEFAULT);
			if (auto external_symbol = file.loader.resolve_symbol(needed_symbol.name(), nullptr, file.ns, &file, mode)) {
				//relocations.insert(reloc, external_symbol.value());
				auto & external_symobj = external_symbol->object();
				LOG_INFO << "Relocating symbol " << needed_symbol.name() << " in " << *this << " with " << external_symbol->name() << " from " << external_symobj << endl;
				return reinterpret_cast<void*>(relocator.fix_external(this->base, external_symbol.value(), external_symobj.base, external_symobj.base + external_symbol->value(), file.tls_module_id, file.tls_offset));
			} else if (needed_symbol.bind() == STB_WEAK) {
				LOG_DEBUG << "Unable to resolve weak symbol " << needed_symbol.name() << "..." << endl;
			} else {
				LOG_ERROR << "Unable to resolve symbol " << needed_symbol.name() << " for relocation..." << file << endl;
			}
		}
	}
	return nullptr;
}

bool ObjectRelocatable::initialize() {
	LOG_DEBUG << "Initialize " << *this << endl;
/*
	// Preinit array
	for (auto & section : this->sections)
		if (section.size() > 0 && section.type() == SHT_PREINIT_ARRAY) {
			auto * f = reinterpret_cast<Elf::DynamicTable::func_init_t *>(base + section.virt_addr());
			for (size_t i = 0; i < section.size() / sizeof(void*); i++)
				f[i](this->file.loader.argc, this->file.loader.argv, this->file.loader.envp);
		}

	// init func
	auto init = symbols["_init"];
	if (init.type() == Elf::STT_FUNC) {
		auto f = reinterpret_cast<Elf::DynamicTable::func_init_t>(base + init.value());
		f(file.loader.argc, file.loader.argv, file.loader.envp);
	} else {
		// In case there is no _init function, we have to use the .init function
		for (auto & section : this->init_sections)
			if (strcmp(section.name(), ".init") == 0) {
				assert(section.size() > 0 && section.type() == SHT_PROGBITS);
				auto f = reinterpret_cast<Elf::DynamicTable::func_init_t>(base + section.virt_addr());
				f(file.loader.argc, file.loader.argv, file.loader.envp);
			}
	}

	// init array
	for (auto & section : this->sections)
		if (section.size() > 0 && section.type() == SHT_INIT_ARRAY) {
			auto * f = reinterpret_cast<Elf::DynamicTable::func_init_t *>(base + section.virt_addr());
			for (size_t i = 0; i < section.size() / sizeof(void*); i++)
				f[i](file.loader.argc, file.loader.argv, file.loader.envp);
		}
*/
	return true;
}
