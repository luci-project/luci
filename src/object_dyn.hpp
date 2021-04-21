#pragma once

#include <string>
#include <vector>

#include <elf.hpp>

#include "object_rel.hpp"
#include "object_exec.hpp"

struct ObjectDynamic : public ObjectExecutable {

	ObjectDynamic(std::string path, int fd, void * mem, DL::Lmid_t ns)
	  : ObjectExecutable(path, fd, mem, ns),
	    dynamic(find_dynamic()),
	    dynamic_symbols(find_dynamic_symbol_table()),
	    dynamic_relocations(find_dynamic_relocation()),
	    version_needed(get_section(Elf::DT_VERNEED).get_list<Elf::VersionNeeded>(true)),
	    version_definition(get_section(Elf::DT_VERDEF).get_list<Elf::VersionDefinition>(true))
	{
		assert(dynamic_relocations.empty() || reinterpret_cast<uintptr_t>(elf.sections[dynamic_relocations[0].symtab].data()) == dynamic_symbols.address());
	}

	Symbol dynamic_symbol(const char * name, const char * version = nullptr) const;

	void* resolve(size_t index) const override;

 protected:
	bool preload() override;

	/*! \brief load required libaries */
	bool preload_libraries();

	bool relocate() override;

	Symbol symbol(const Symbol & sym) const override;

 private:
	Elf::Array<Elf::Dynamic> dynamic;
	Elf::SymbolTable dynamic_symbols;
	Elf::Array<Elf::Relocation> dynamic_relocations;
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
				return Elf::SymbolTable(this->elf, section, version_section);
			}
		return Elf::SymbolTable(this->elf);
	}

	Elf::Array<Elf::Relocation> find_dynamic_relocation() const {
		uintptr_t jmprel = 0;  // Address of PLT relocation table
		uintptr_t pltrel = Elf::DT_NULL;  // Type of PLT relocation table (REL or RELA)
		size_t pltrelsz = 0;   // Size of PLT relocation table (and, hence, GOT)

		for (auto &dyn: dynamic) {
			switch (dyn.tag()) {
				case Elf::DT_JMPREL:
					jmprel = dyn.value();
					break;
				case Elf::DT_PLTREL:
					pltrel = dyn.value();
					break;
				case Elf::DT_PLTRELSZ:
					pltrelsz = dyn.value();
					break;
				default:
					continue;
			}
		}

		if (pltrel != Elf::DT_NULL) {
			assert(jmprel != 0);
			auto section = this->elf.section_by_offset(jmprel);
			LOG_INFO << jmprel; //elf.sections.index(section);
			assert(section.size() == pltrelsz);
			return section.get_relocations();
		} else {
			assert(jmprel == 0);
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
			return Symbol::Version(true);

		for (auto & v : version_needed)
			for (auto & aux : v.auxiliary())
				if (aux.version_index() == index)
					return Symbol::Version(aux.name(), aux.hash(), aux.weak());

		for (auto & v : version_definition)
			if (v.version_index() == index && !v.base())
				return  Symbol::Version(v.auxiliary()[0].name(), v.hash(), v.weak());

		return Symbol::Version(false);
	}
};
