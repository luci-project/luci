#pragma once

#include <dlh/container/vector.hpp>
#include <dlh/utils/log.hpp>

#include "object/identity.hpp"
#include "object/relocatable.hpp"
#include "object/executable.hpp"

#include "versioned_symbol.hpp"


struct ObjectDynamic : public ObjectExecutable {
	/*! \brief
	 * \param mapped Mapped to virtual memory location specified in segments
	 */
	ObjectDynamic(ObjectIdentity & file, const Object::Data & data, bool mapped = false)
	  : ObjectExecutable{file, data},
	    dynamic_table{this->dynamic(mapped)},
	    dynamic_symbols{dynamic_table.get_symbol_table()},
	    dynamic_relocations{dynamic_table.get_relocations()},
	    dynamic_relocations_plt{dynamic_table.get_relocations_plt()},
	    version_needed{dynamic_table.get_version_needed()},
	    version_definition{dynamic_table.get_version_definition()}
	{
		// Set Globale Offset Table Pointer
		auto got = dynamic_table[Elf::DT_PLTGOT];
		if (got.valid() && got.tag() == Elf::DT_PLTGOT)
			global_offset_table = got.value();

		// Check (set) soname
		StrPtr soname(dynamic_table.get_soname());
		if (!soname.empty() && soname != file.name) {
			if (!file.name.empty())
				LOG_WARNING << "Library file name (" << file.name << ") differs from soname (" << soname << ") -- using latter one!" << endl;
			file.name = soname;
		}
	}

	void* dynamic_resolve(size_t index) const override;

 protected:
	bool preload() override;

	/*! \brief load required libaries */
	bool preload_libraries();

	bool prepare() override;

	void update() override;

	bool patchable() const override;

	Optional<VersionedSymbol> resolve_symbol(const VersionedSymbol & sym) const override;
	Optional<VersionedSymbol> resolve_symbol(uintptr_t addr) const override;

	void* relocate(const Elf::Relocation & reloc) const;

	bool initialize() override;

 private:
	Elf::DynamicTable dynamic_table;
	Elf::SymbolTable dynamic_symbols;
	Elf::Array<Elf::Relocation> dynamic_relocations, dynamic_relocations_plt;
	Elf::List<Elf::VersionNeeded> version_needed;
	Elf::List<Elf::VersionDefinition> version_definition;

	ObjectDynamic(const ObjectDynamic&) = delete;
	ObjectDynamic& operator=(const ObjectDynamic&) = delete;

	uint16_t version_index(const VersionedSymbol::Version & version) const {
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

	VersionedSymbol::Version version(uint16_t index) const {
		if (index == Elf::VER_NDX_GLOBAL)
			return VersionedSymbol::Version{true};

		for (auto & v : version_needed)
			for (auto & aux : v.auxiliary())
				if (aux.version_index() == index)
					return VersionedSymbol::Version{aux.name(), aux.hash(), aux.weak()};

		for (auto & v : version_definition)
			if (v.version_index() == index && !v.base())
				return  VersionedSymbol::Version{v.auxiliary()[0].name(), v.hash(), v.weak()};

		return VersionedSymbol::Version{false};
	}
};
