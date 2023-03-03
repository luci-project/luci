#pragma once

#include <dlh/container/vector.hpp>
#include <dlh/log.hpp>

#include "object/identity.hpp"
#include "object/relocatable.hpp"
#include "object/executable.hpp"

#include "symbol.hpp"


struct ObjectDynamic : public ObjectExecutable {
	/*! \brief
	 * \param mapped Mapped to virtual memory location specified in segments
	 */
	ObjectDynamic(ObjectIdentity & file, const Object::Data & data, bool position_independent = true);

	void* dynamic_resolve(size_t index) const override;

 protected:
	using ObjectExecutable::resolve_symbol;

	bool use_data_alias() const override {
		return this->file.flags.updatable == 1;
	};

	bool preload() override;

	bool fix() override;

	/*! \brief load required libaries */
	bool preload_libraries();

	/*! \brief configure glibc stuff */
	bool compatibility_setup();

	bool prepare() override;

	bool update() override;

	bool patchable() const override;

	Optional<VersionedSymbol> resolve_symbol(const char * name, uint32_t hash, uint32_t gnu_hash, const VersionedSymbol::Version & version) const override;
	Optional<VersionedSymbol> resolve_symbol(uintptr_t addr) const override;

	void* relocate(const Elf::Relocation & reloc, bool fix, bool & fatal) const;
	void* relocate(const Elf::Relocation & reloc, bool fix) const {
		bool fatal;
		return relocate(reloc, fix, fatal);
	}

	bool initialize(bool preinit = false) override;

 private:
	Elf::DynamicTable dynamic_table;
	Elf::SymbolTable dynamic_symbols;
	Elf::Array<Elf::Relocation> dynamic_relocations, dynamic_relocations_plt;
	Elf::List<Elf::VersionNeeded> version_needed;
	Elf::List<Elf::VersionDefinition> version_definition;

	ObjectDynamic(const ObjectDynamic&) = delete;
	ObjectDynamic& operator=(const ObjectDynamic&) = delete;

	void addpath(Vector<const char *> & vec, const char * str);

	/*! \brief check if relocation modifies (shared) data section */
	bool in_data(const Elf::Relocation & reloc) const;

	uint16_t version_index(const VersionedSymbol::Version & version) const {
		if (!version.valid) {
			//return Elf::VER_NDX_LOCAL;
		} else if (version.name != nullptr) {

			// Version Definition Section
			bool skip_version_definition = false;
			if (version.file != nullptr)
				for (auto & v : version_definition)
					if (v.base()) {
						skip_version_definition = version.hash != v.hash() || strcmp(v.auxiliary().at(0).name(), version.file) != 0;
						break;
					}
			if (!skip_version_definition)
				for (auto & v : version_definition)
					if (v.hash() == version.hash && !v.base()) {
						const char * n = v.auxiliary()[0].name();
						if (n == version.name || strcmp(n, version.name) == 0)
							return v.version_index();
					}

			// Version Dependency Section
			for (auto & v : version_needed)
				if (version.file == nullptr || strcmp(v.file(), version.file) == 0)
					for (auto & aux : v.auxiliary())
						if (aux.hash() == version.hash && (aux.name() == version.name || strcmp(aux.name(), version.name) == 0))
							return aux.version_index();

		}

		return Elf::VER_NDX_GLOBAL;
	}

	VersionedSymbol::Version get_version(uint16_t index) const {
		if (index == Elf::VER_NDX_GLOBAL)
			return VersionedSymbol::Version{true};

		// Version Dependency Section
		for (auto & v : version_needed)
			for (auto & aux : v.auxiliary())
				if (aux.version_index() == index)
					return VersionedSymbol::Version{aux.name(), aux.hash(), aux.weak(), v.file()};

		// Version Definition Section
		const char * file = nullptr;
		uint32_t filehash = 0;
		for (auto & v : version_definition)
			if (v.base()) {
				file = v.auxiliary().at(0).name();
				filehash = v.hash();
			}
		for (auto & v : version_definition)
			if (v.version_index() == index && !v.base())
				return VersionedSymbol::Version{v.auxiliary().at(0).name(), v.hash(), v.weak(), file, filehash};

		return VersionedSymbol::Version{false};
	}
};
