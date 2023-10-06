// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "object/relocatable.hpp"

#include <dlh/log.hpp>
#include <dlh/is_in.hpp>
#include <dlh/string.hpp>
#include <dlh/auxiliary.hpp>

#include <elfo/elf_rel.hpp>

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

// uintptr_t ObjectRelocatable::offset = 0;

ObjectRelocatable::ObjectRelocatable(ObjectIdentity & file, const Object::Data & data)
  : Object{file, data} {
	// Updates require rewriting of old files
	if (file.flags.updatable)
		file.flags.update_outdated = 1;

	auto address = file.loader.next_address();
	if (file.current == nullptr) {
		base = address;
		offset = 0;
	} else {
		base = file.current->base;
		assert(address > base);
		offset = address - base;
		// Adress offsets are signed > 1 GB -- this could be an issue
		if (offset > 1UL * 1024 * 1024 * 1024) {
			if (offset > 2UL * 1024 * 1024 * 1024) {
				LOG_ERROR << "Beyond two giga byte limit -- this *will* fail!" << endl;
			} else {
				LOG_WARNING << "Beyond one giga byte!" << endl;
			}
		}
	}
}


bool ObjectRelocatable::preload() {
	// TODO flags.immutable_source  (should always be set, right?)

	Optional<Elf::Section> tdata;
	Optional<Elf::Section> tbss;

	// Check availability of external symbols on updates
	if (this->file_previous != nullptr && !file.loader.config.force_update)
		for (const auto & sym : symbol_table())
			if (sym.undefined() && sym.bind() == STB_GLOBAL && !file.loader.resolve_symbol(sym.name(), nullptr, file.ns, &file).has_value()) {
				LOG_WARNING << "Missing external symbol " << sym.name() << " -- abort updating object!" << endl;
				return false;
			}

	for (const auto & section : this->sections) {
		if (section.size() == 0)
			continue;
		if (section.tls() && section.allocate()) {
			// Fixup section entry,
			assert(section.virt_addr() == 0);
			Elf::Shdr * data = const_cast<Elf::Shdr *>(section.ptr());
			data->sh_addr = offset;

			if (section.type() == SHT_NOBITS) {
				tbss = section;
			} else {
				tdata = section;
				// Mapping
				auto mem = memory_map.emplace_back(*this, section, offset, 0, base);
				// increase offset to next free page
				offset += mem->target.page_size();
			}
			continue;
		}

		switch (section.type()) {
			case SHT_NOBITS:
			case SHT_PROGBITS:
			case SHT_INIT_ARRAY:
			case SHT_FINI_ARRAY:
			case SHT_PREINIT_ARRAY:
				if (section.allocate()) {
					size_t additional_size = 0;
					if (this->file_previous == nullptr && section.type() == SHT_PROGBITS && (strcmp(section.name(), ".init", 5) == 0 || strcmp(section.name(), ".fini", 5) == 0)) {
						// The init/fini section consists only from calls.
						// They need to be fixed after mapping
						init_sections.push_back(section);
						// We need an additional byte for the return instruction
						additional_size = 1;
					}

					// Fixup section entry,
					assert(section.virt_addr() == 0);
					Elf::Shdr * data = const_cast<Elf::Shdr *>(section.ptr());
					data->sh_addr = offset;

					// Fixup symbols & relocations
					bool changed = adjust_offsets(offset, section);

					// If the whole writeable data/bss can be used from previous sections,
					// there is no need to allocate it
					if (this->file_previous == nullptr || changed || !section.writeable()) {
						// Create mapping
						auto mem = memory_map.emplace_back(*this, section, offset, additional_size, base);

						// increase offset to next free page
						offset += mem->target.page_size();
					}
				}
				break;

			case SHT_REL:
			case SHT_RELA:
				// Only relocations for allocated sections (linked in the info attribute)
				if (section.info() != 0 && this->sections[section.info()].allocate())
					relocation_tables.push_back(section.get_relocations());
				break;
		}
	}

	// Setup TLS
	if ((tdata.has_value() || tbss.has_value()) && this->file.tls_module_id == 0) {
		size_t tls_size = 0;
		size_t tls_alignment = 0;
		uintptr_t tls_image = 0;
		size_t tls_image_size = 0;

		if (tdata.has_value()) {
			tls_size = tdata->size();
			tls_alignment = tdata->alignment();
			tls_image = tdata->virt_addr();
			tls_image_size = tdata->size();
		}

		if (tbss.has_value()) {
			if (tls_size != 0) {
				Elf::Shdr * data = const_cast<Elf::Shdr *>(tdata->ptr());
				data->sh_addr = tls_size;
				// Fixup BSS symbols & relocations
				adjust_offsets(tls_size, tbss.value());
			}
			tls_size += tdata->size();
			// Adjust BSS
			tls_alignment = Math::max(tls_alignment, tdata->alignment());
		}

		this->file.tls_module_id = this->file.loader.tls.add_module(this->file, tls_size, tls_alignment, tls_image, tls_image_size, this->file.tls_offset);
	}

	return !memory_map.empty();
}


bool ObjectRelocatable::fix() {
	// TODO: add _start routine etc
	return true;
}


bool ObjectRelocatable::adjust_offsets(uintptr_t offset, const Elf::Section & section) {
	int64_t index = static_cast<int64_t>(this->sections.index(section));
	bool changed = false;

	// Relocations
	for (const auto & linked : this->sections) {
		if (linked.size() == 0)
			continue;

		switch (linked.type()) {
			case SHT_SYMTAB:
				// fix symbols
				for (auto sym : linked.get_symbol_table())
					if (sym.section_index() == index) {
						// For updated versions, check previous writable data sections
						if (this->file_previous != nullptr && section.writeable()) {
							auto prev_sym = this->file_previous->resolve_internal_symbol(sym.name());
							// if we have an old matching symbol, use its address
							if (prev_sym.has_value() && prev_sym->type() == sym.type() && prev_sym->size() == sym.size()) {
								Elf::Sym * new_data = const_cast<Elf::Sym *>(sym.ptr());
								// Fixup value (= offset)
								new_data->st_value = prev_sym->value();
								// Insert into lookup list
								symbols.insert(prev_sym.value());
								continue;
							}
						}
						// For general symbols adjust entry
						if (is(sym.type()).in(STT_OBJECT, STT_FUNC, STT_SECTION, STT_GNU_IFUNC, STT_TLS)) {
							// This is only allowed since we have a COW mapping
							Elf::Sym * data = const_cast<Elf::Sym *>(sym.ptr());
							// Fixup value (= offset)
							data->st_value += offset;
							// Add to symbol list
							symbols.insert(sym);
							// Store change of symbol table
							changed = true;
						}
					}
				break;

			case SHT_REL:
				// Fixup relocations to this section
				if (linked.info_link() && static_cast<int16_t>(linked.info()) == index)
					for (auto rel : linked.get_array<RelocationWithoutAddend>()) {
						// This is only allowed since we have a COW mapping
						Elf::Rel * data = const_cast<Elf::Rel *>(rel.ptr());
						// Fixup offset
						data->r_offset += offset;
						// Store change of relocation table
						changed = true;
					}
				break;

			case SHT_RELA:
				// Fixup relocations to this section
				if (linked.info_link() && static_cast<int16_t>(linked.info()) == index)
					for (auto rel : linked.get_array<RelocationWithAddend>()) {
						// This is only allowed since we have a COW mapping
						Elf::Rela * data = const_cast<Elf::Rela *>(rel.ptr());
						// Fixup offset
						data->r_offset += offset;
						// Store change of relocation table
						changed = true;
					}
				break;
		}
	}
	return changed;
}


bool ObjectRelocatable::prepare() {
	LOG_INFO << "Prepare " << *this << " with " << reinterpret_cast<void*>(global_offset_table) << endl;
	bool success = true;

	// Allocate bss space for tentative definitions
	// this has to be done after all other relocatable objects have been preloaded
	size_t global_bss = 0;
	// Check symbol table for tentative definitions
	for (const auto & sym : symbol_table())
		if (sym.size() != 0 && sym.section_index() == Elf::SHN_COMMON) {
			bool found = false;
			for (const auto & object_file : file.loader.lookup) {
				auto init_sym = object_file.current->resolve_internal_symbol(sym.name());
				if (init_sym.has_value() && init_sym->type() == sym.type() && init_sym->size() == sym.size()) {
					Elf::Sym * data = const_cast<Elf::Sym *>(sym.ptr());
					// Fixup value (= offset)
					data->st_value = object_file.base + init_sym->value() - base;
					// Insert into lookup list
					symbols.insert(init_sym.value());
					// Mark found
					found = true;
					break;
				}
			}
			if (!found) {
				// Check if such a symbol was already defined within
				// Value is alignment
				auto start = Math::align_up(global_bss, sym.value());
				// Add to global bss size
				global_bss = start + sym.size();
				// This is only allowed since we have a COW mapping
				Elf::Sym * data = const_cast<Elf::Sym *>(sym.ptr());
				// Fixup value (= offset)
				data->st_value = offset + start;
				// Add to symbol list
				symbols.insert(sym);
			}
		}
	// Allocate
	if (global_bss > 0) {
		auto mem = memory_map.emplace_back(*this, offset, global_bss, base);
		// increase offset to next free page
		offset += mem->target.page_size();
		// And map target
		mem->map();
	}


	// Perform initial relocations
	for (auto & relocations : relocation_tables)
		for (const auto & reloc : relocations) {
			// LOG_INFO << "Relocate " << (void*)reloc.offset() << " = " << (void*)reloc.symbol().address() << endl;
			if (relocate(reloc) == nullptr)
				success = false;
		}

	// Add ret statement to section which need fixing
	for (auto & section : this->init_sections) {
		assert(section.size() > 0 && section.type() == SHT_PROGBITS);
		char * end = reinterpret_cast<char *>(base + section.virt_addr() + section.size());
		LOG_INFO << "Add retq instruction to " << reinterpret_cast<void*>(end) << " at " << section.name() << endl;
		*end = static_cast<char>(0xc3);  // retq
	}

	for (const auto & m : memory_map)
		m.dump();

	return success;
}


bool ObjectRelocatable::update() {
	for (const auto & r : relocations) {
		// Update only if target has changed (relocation does not point to latest version).
		// And if this is not the latest version, then omit (shared) data section relocations
		// since they are performed in the latest version of this object
		if (!r.value.object().is_latest_version() && is(r.value.type()).in(STT_FUNC, STT_SECTION))
			relocate(r.key);
	}

	return true;
}


bool ObjectRelocatable::patchable() const {
	// check if updateable
	return file_previous != nullptr
	    && file_previous->header.identification == this->header.identification
	    && file_previous->header.type()         == this->header.type()
	    && file_previous->header.machine()      == this->header.machine()
	    && file_previous->header.version()      == this->header.version();
	// TODO: TLS must not change
}


Optional<ElfSymbolHelper> ObjectRelocatable::resolve_internal_symbol(const SymbolHelper & needle) const {
	const auto sym = symbols.find(needle);
	if (sym != symbols.end() && !sym->undefined()) {
		return Optional<ElfSymbolHelper>{ *sym };
	}

	// hand over to previous version to resurrect old data fields -- TODO is this intuitive or counter intuitive?
	// if (file_previous != nullptr)
	// 	return reinterpret_cast<ObjectRelocatable*>(this->file_previous)->resolve_internal_symbol(needle);
	return Optional<ElfSymbolHelper>{};
}


Optional<VersionedSymbol> ObjectRelocatable::resolve_symbol(const char * name, uint32_t hash, uint32_t gnu_hash, const VersionedSymbol::Version & version) const {
	(void) hash;
	(void) version;
	const auto sym = symbols.find(SymbolHelper{name, gnu_hash});
	if (sym != symbols.end() && !sym->undefined() && sym->bind() != Elf::STB_LOCAL && sym->visibility() == Elf::STV_DEFAULT) {
		VersionedSymbol vs{ *sym };
		return Optional<VersionedSymbol>{ vs };
	}
	return Optional<VersionedSymbol>{};
}


Optional<VersionedSymbol> ObjectRelocatable::resolve_symbol(uintptr_t addr) const {
	if (addr > base) {
		uintptr_t offset = addr - base;
		for (const auto & sym : symbols)
			if (!sym.undefined() && offset >= sym.value() && offset <= sym.value() + sym.size()) {
				VersionedSymbol vs{sym};
				return Optional<VersionedSymbol>{ vs };
			}
	}
	return Optional<VersionedSymbol>{};
}

void* ObjectRelocatable::relocate(const Elf::Relocation & reloc) const {
	// Initialize relocator object
	Relocator relocator(reloc, 0);

	// Since we have only a few segments, iterating is just fine
	MemorySegment * seg = nullptr;
	/*for (auto &mem : memory_map)
		if (mem.target.contains(relocator.address(this->base))) {
			seg = &mem;
			break;
		}
*/
	// Special case: reading address of GOTPCRELX
	if (is(reloc.type()).in(Elf::R_X86_64_REX_GOTPCRELX, Elf::R_X86_64_GOTPCRELX)) {
		auto * instruction = reinterpret_cast<uint8_t*>(this->base + (seg != nullptr ? seg->compose() - seg->target.address() : 0) + reloc.offset() - 2);
		// check if instruction is MOV (+ register)
		if (*instruction == 0x8b) {
			LOG_DEBUG << "Rewriting MOV with GOTPCRELX relocation to LEA at " << reinterpret_cast<void*>(instruction) << endl;
			// Addend must be -4
			assert(reloc.addend() == -4);
			// Change to LEA
			*instruction += 2;
		}
	}
	// find symbol
	if (reloc.symbol_index() == 0) {
		// Local relocation (without symbol) -- not sure if can occure at all
		LOG_INFO << "Relocating local in " << *this <<  endl;
		return reinterpret_cast<void*>(relocator.fix_internal(this->base, 0, this->file.tls_module_id, this->file.tls_offset));
	} else {
		const auto needed_symbol = reloc.symbol();
		// We omit the PLT and directly link the target, hence replacing `plt_entry` with `base + symbol.value()``
		auto plt_entry = base + needed_symbol.value();
		if (!needed_symbol.undefined()) {
			// Special case: We have a newer version of a function
			if (!is_latest_version() && is(needed_symbol.type()).in(STT_FUNC, STT_GNU_IFUNC, STT_SECTION)) {
				auto latest_symbol = this->file.current->resolve_internal_symbol(needed_symbol.name());
				if (latest_symbol.has_value() && needed_symbol.type() == latest_symbol->type()) {
					relocations.insert(reloc, latest_symbol.value());
					auto value = relocator.value_external(this->base, latest_symbol.value(), file.current->base, file.current->base + latest_symbol->value(), this->file.tls_module_id, this->file.tls_offset);
					LOG_TRACE << "Updating symbol " << needed_symbol.name() << " in " << *this << " with " << latest_symbol->name() << " from " << *file.current << " to " << reinterpret_cast<void*>(value) << endl;
					return reinterpret_cast<void*>(relocator.fix_value_external(this->base + (seg != nullptr ? seg->compose() - seg->target.address() : 0), latest_symbol.value(), value));
				}
			}
			// Local symbol
			relocations.insert(reloc, needed_symbol);
			auto value = relocator.value_internal(this->base, plt_entry, this->file.tls_module_id, this->file.tls_offset);
			LOG_TRACE << "Relocating local symbol " << needed_symbol.name() << " @ " << reinterpret_cast<void*>(base + needed_symbol.value()) << " in " << *this << " to " << reinterpret_cast<void*>(value) << endl;
			return reinterpret_cast<void*>(relocator.fix_value_internal(this->base + (seg != nullptr ? seg->compose() - seg->target.address() : 0), value));
		} else {
			// external symbol
			// COPY Relocations have a defined symbol with the same name
			Loader::ResolveSymbolMode mode = relocator.is_copy() ? Loader::RESOLVE_EXCEPT_OBJECT : (file.flags.bind_deep == 1 ? Loader::RESOLVE_OBJECT_FIRST : Loader::RESOLVE_DEFAULT);
			if (auto external_symbol = file.loader.resolve_symbol(needed_symbol.name(), nullptr, file.ns, &file, mode)) {
				relocations.insert(reloc, external_symbol.value());
				const auto & external_symobj = external_symbol->object();
				auto value = relocator.value_external(this->base, external_symbol.value(), external_symobj.base, external_symobj.base + external_symbol->value(), file.tls_module_id, file.tls_offset);
				LOG_TRACE << "Relocating symbol " << needed_symbol.name() << " in " << *this << " with " << external_symbol->name() << " from " << external_symobj << " to " << reinterpret_cast<void*>(value) << endl;
				return reinterpret_cast<void*>(relocator.fix_value_external(this->base + (seg != nullptr ? seg->compose() - seg->target.address() : 0), external_symbol.value(), value));
			} else if (needed_symbol.bind() == STB_WEAK) {
				LOG_DEBUG << "Unable to resolve weak symbol " << needed_symbol.name() << "..." << endl;
			} else {
				LOG_ERROR << "Unable to resolve symbol " << needed_symbol.name() << " for relocation..." << file << endl;
			}
		}
	}

	// TODO: Weak symbols!
	return nullptr;
}

bool ObjectRelocatable::initialize(bool preinit) {
	if (preinit) {
		LOG_DEBUG << "Preinitialize " << *this << endl;
		// Preinit array
		for (const auto & section : this->sections)
			if (section.size() > 0 && section.type() == SHT_PREINIT_ARRAY) {
				auto * f = reinterpret_cast<Elf::DynamicTable::func_init_t *>(base + section.virt_addr());
				for (size_t i = 0; i < section.size() / sizeof(void*); i++)
					f[i](this->file.loader.argc, this->file.loader.argv, this->file.loader.envp);
			}
	} else {
		LOG_DEBUG << "Initialize " << *this << endl;
		// init func
		auto init = symbols.find(SymbolHelper{"_init"});
		if (init != symbols.end() && init->type() == Elf::STT_FUNC) {
			auto f = reinterpret_cast<Elf::DynamicTable::func_init_t>(base + init->value());
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
		for (const auto & section : this->sections)
			if (section.size() > 0 && section.type() == SHT_INIT_ARRAY) {
				auto * f = reinterpret_cast<Elf::DynamicTable::func_init_t *>(base + section.virt_addr());
				for (size_t i = 0; i < section.size() / sizeof(void*); i++) {
					f[i](file.loader.argc, file.loader.argv, file.loader.envp);
				}
			}
	}
	return true;
}
