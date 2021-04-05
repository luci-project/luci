#pragma once

#include <string>
#include <vector>

#include <elf.hpp>

#include "object_rel.hpp"
#include "object_exec.hpp"

struct ObjectDynamic : public ObjectExecutable, public ObjectRelocatable {

	ObjectDynamic(std::string path, int fd, void * mem)
	  : Object(path, fd, mem),
	    dynamic(find_dynamic()),
	    dynamic_symbols(find_dynamic_symbol_table()),
	    version_needed(get_section(Elf::DT_VERNEED).get_list<Elf::VersionNeeded>(true)),
	    version_definition(get_section(Elf::DT_VERDEF).get_list<Elf::VersionDefinition>(true))
	{}

	Symbol dynamic_symbol(const char * name, const char * version = nullptr) const;

 protected:
	bool preload() override;

	/*! \brief load required libaries */
	bool preload_libraries();

	bool relocate() override;

	Symbol symbol(const Symbol & sym) const override;

	uintptr_t global_offset_table = 0;

 private:
	Elf::Array<Elf::Dynamic> dynamic;
	Elf::SymbolTable dynamic_symbols;
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

 	Elf::Array<Elf::Dynamic> find_dynamic() const {
 		for (auto & section : elf.sections)
 			if (section.type() == Elf::SHT_DYNAMIC)
 				return section.get_dynamic();
 		return elf.get<Elf::Dynamic>();
 	}

	bool has_dynamic_value(Elf::dyn_tag tag, uintptr_t & value) const {
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

	uint16_t version_index(const char * name, const uint32_t hash, bool & weak) const {
		if (name != nullptr) {
			for (auto & v : version_needed)
				for (auto & aux : v.auxiliary())
					if (aux.hash() == hash && strcmp(aux.name(), name) == 0) {
						weak = aux.weak();
						return aux.version_index();
					}

			for (auto & v : version_definition)
				if (v.hash() == hash && !v.base() &&  strcmp(v.auxiliary()[0].name(), name) == 0) {
					weak = v.weak();
					return v.version_index();
				}
		}

		return Elf::VER_NDX_UNKNOWN;
	}

	uint16_t version_index(const char * name, bool & weak) const {
		return version_index(name, version_hash(name), weak);
	}

	uint16_t version_index(const char * name) const {
		bool weak;
		return version_index(name, weak);
	}

	static uint32_t version_hash(const char * name) {
		return ELF_Def::hash(name);
	}

	const char * version_name(uint16_t index, bool & weak) const {
		switch (index) {
			case Elf::VER_NDX_LOCAL:
				return "*local*";
			case Elf::VER_NDX_GLOBAL:
				return "*global*";
			case Elf::VER_NDX_ELIMINATE:
				return "*eliminate*";
			case Elf::VER_NDX_UNKNOWN:
				return "*unknown*";
		}

		for (auto & v : version_needed)
			for (auto & aux : v.auxiliary())
				if (aux.version_index() == index) {
					weak = aux.weak();
					return aux.name();
				}

		for (auto & v : version_definition)
			if (v.version_index() == index && !v.base()) {
				weak = v.weak();
				return v.auxiliary()[0].name();
			}

		return "*invalid*";
	}

	const char * version_name(uint16_t index) const {
		bool weak;
		return version_name(index, weak);
	}
};
