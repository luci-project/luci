#pragma once

#include <vector>

#include <elf.hpp>

#include "object_rel.hpp"
#include "object_exec.hpp"

struct ObjectDynamic : public ObjectExecutable {
	ObjectDynamic(ObjectFile & file, const Object::Data & data)
	  : ObjectExecutable{file, data},
	    dynamic{find_dynamic()},
	    dynamic_symbols{find_dynamic_symbol_table()},
	    dynamic_relocations{find_relocation_table(false)},
	    dynamic_relocations_plt{find_relocation_table(true)},
	    version_needed{get_section(Elf::DT_VERNEED).get_list<Elf::VersionNeeded>(true)},
	    version_definition{get_section(Elf::DT_VERDEF).get_list<Elf::VersionDefinition>(true)}
	{
		assert(dynamic_relocations.empty() || reinterpret_cast<uintptr_t>(elf.sections[dynamic_relocations[0].symtab].data()) == dynamic_symbols.address());
		assert(dynamic_relocations_plt.empty() || reinterpret_cast<uintptr_t>(elf.sections[dynamic_relocations_plt[0].symtab].data()) == dynamic_symbols.address());
	}

	void* dynamic_resolve(size_t index) const override;

 protected:
	bool preload() override;

	/*! \brief load required libaries */
	bool preload_libraries();

	bool prepare() override;

	bool patchable() const override;

	std::optional<Symbol> resolve_symbol(const Symbol & sym) const override;

	void* relocate(const Elf::Relocation & reloc) const;

	bool initialize() override {
		init.run();
		return true;
	};

 private:
	Elf::Array<Elf::Dynamic> dynamic;
	Elf::SymbolTable dynamic_symbols;
	Elf::Array<Elf::Relocation> dynamic_relocations, dynamic_relocations_plt;
	Elf::List<Elf::VersionNeeded> version_needed;
	Elf::List<Elf::VersionDefinition> version_definition;

	struct {
		void (*func)() = nullptr;
		void (**func_prearray)() = nullptr;
		void (**func_array)() = nullptr;
		size_t func_prearray_size = 0;
		size_t func_array_size = 0;

		void run() const {
			for (size_t i = 0; i < func_prearray_size; ++i) {
				(*func_prearray[i])();
			}

			if (func != nullptr) {
				func();
			}

			for (size_t i = 0; i < func_array_size; i++) {
				(*func_array[i])();
			}
		}
	} init, fini;

	ObjectDynamic(const ObjectDynamic&) = delete;
	ObjectDynamic& operator=(const ObjectDynamic&) = delete;

 	Elf::Array<Elf::Dynamic> find_dynamic() const {
 		for (auto & section : elf.sections)
 			if (section.type() == Elf::SHT_DYNAMIC)
 				return section.get_dynamic();
 		return elf.get<Elf::Dynamic>();
 	}

	bool has_dynamic_value(const Elf::dyn_tag tag, uintptr_t & value) const {
		for (auto &dyn: dynamic)
			if (dyn.tag() == tag) {
				value = dyn.value();
				return true;
			}
		return false;
	}

	bool has_dynamic_value(Elf::dyn_tag tag) const {
		uintptr_t value;
		return has_dynamic_value(tag, value);
	}

	Elf::Section get_section(Elf::dyn_tag tag) const {
		// We don't care about O(n) since n is quite small.
		uintptr_t value;
		return has_dynamic_value(tag, value) ? elf.section_by_offset(value) : elf.sections[0];
	}

	Elf::SymbolTable find_dynamic_symbol_table() const {
		Elf::dyn_tag search_tags[] = { Elf::DT_GNU_HASH, Elf::DT_HASH, Elf::DT_SYMTAB };
		uintptr_t offset;
		for (auto & tag : search_tags)
			if (has_dynamic_value(tag, offset)) {
				const Elf::Section section = elf.section_by_offset(offset);
				assert(section.type() == Elf::SHT_GNU_HASH || section.type() == Elf::SHT_HASH || section.type() == Elf::SHT_DYNSYM || section.type() == Elf::SHT_SYMTAB);
				const Elf::Section version_section = get_section(Elf::DT_VERSYM);
				assert(version_section.type() == Elf::SHT_NULL || version_section.type() == Elf::SHT_GNU_VERSYM);
				return Elf::SymbolTable{this->elf, section, version_section};
			}
		return Elf::SymbolTable{this->elf};
	}

	Elf::Array<Elf::Relocation> find_relocation_table(bool plt) const {
		uintptr_t offset = 0;   // Address offset of relocation table
		size_t size = 0;        // Size of relocation table
		size_t entry_size = 0;  // Size of relocation table entry

		for (auto &dyn: dynamic) {
			if (plt)
				switch (dyn.tag()) {
					case Elf::DT_JMPREL:
						offset = dyn.value();
						break;
					case Elf::DT_PLTREL:
						switch (dyn.value()) {
							case Elf::DT_REL:
								entry_size = sizeof(*Elf::RelocationWithoutAddend::_data);
								break;
							case Elf::DT_RELA:
								entry_size = sizeof(*Elf::RelocationWithAddend::_data);
								break;
							default:
								assert(false);
						}
						break;
					case Elf::DT_PLTRELSZ:
						size = dyn.value();
						break;
					default:
						continue;
				}
			else
				switch (dyn.tag()) {
					case Elf::DT_REL:
					case Elf::DT_RELA:
						offset = dyn.value();
						break;
					case Elf::DT_RELENT:
					case Elf::DT_RELAENT:
						entry_size = dyn.value();
						break;
					case Elf::DT_RELSZ:
					case Elf::DT_RELASZ:
						size = dyn.value();
						break;
					default:
						continue;
				}
		}

		if (offset != 0) {
			auto section = this->elf.section_by_offset(offset);
			assert(section.size() == size);
			auto r = section.get_relocations();
			assert(r.accessor().element_size() == entry_size);
			return r;
		} else {
			return elf.sections[0].get_array<Elf::Relocation>();
		}
	}

	uint16_t version_index(const Symbol::Version & version) const {
		if (!version.valid) {
			return Elf::VER_NDX_LOCAL;
		} else if (version.name != nullptr) {
			for (auto & v : version_needed)
				for (auto & aux : v.auxiliary())
					if (aux.hash() == version.hash && (aux.name() == version.name || strcmp(aux.name(), version.name) == 0))
						return aux.version_index();

			for (auto & v : version_definition) {
				if (v.hash() == version.hash && !v.base()) {
					const char * n = v.auxiliary()[0].name();
					if (n == version.name || strcmp(n, version.name) == 0)
						return v.version_index();
				}
			}
		}

		return Elf::VER_NDX_GLOBAL;
	}

	Symbol::Version version(uint16_t index) const {
		if (index == Elf::VER_NDX_GLOBAL)
			return Symbol::Version{true};

		for (auto & v : version_needed)
			for (auto & aux : v.auxiliary())
				if (aux.version_index() == index)
					return Symbol::Version{aux.name(), aux.hash(), aux.weak()};

		for (auto & v : version_definition)
			if (v.version_index() == index && !v.base())
				return  Symbol::Version{v.auxiliary()[0].name(), v.hash(), v.weak()};

		return Symbol::Version{false};
	}
};
