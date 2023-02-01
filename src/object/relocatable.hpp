#pragma once

#include <dlh/container/vector.hpp>

#include "object/identity.hpp"
#include "object/base.hpp"

struct ObjectRelocatable : public Object {
	ObjectRelocatable(ObjectIdentity & file, const Object::Data & data);

 protected:
	using Object::resolve_symbol;

	/*! \brief Assign memory locations to sections */
	bool preload() override;

	/*! \brief Luci specific fixes performed right after mapping */
	bool fix() override;

	/*! \brief Relocate sections */
	bool prepare() override;

	bool update() override;

	bool patchable() const override;

	Optional<VersionedSymbol> resolve_symbol(const char * name, uint32_t hash, uint32_t gnu_hash, const VersionedSymbol::Version & version) const override;
	Optional<VersionedSymbol> resolve_symbol(uintptr_t addr) const override;

	void* relocate(const Elf::Relocation & reloc) const;

	bool initialize() override;

 private:
	static uintptr_t offset;
	//static uintptr_t base;

	Vector<Elf::Array<Elf::Relocation>> relocation_tables;
	Vector<Elf::Section> init_sections;
	Elf::SymbolTable symbols;

	/*! \brief Fixup symbols & relocations after assigning an offset to a section*/
	void adjust_offsets(int16_t index, uintptr_t offset);

};
