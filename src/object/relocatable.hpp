// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <dlh/container/vector.hpp>
#include <dlh/string.hpp>
#include <elfo/elf.hpp>

#include "object/identity.hpp"
#include "object/base.hpp"
#include "symbol.hpp"

struct ObjectRelocatable : public Object {
	ObjectRelocatable(ObjectIdentity & file, const Object::Data & data);

 protected:
	using Object::resolve_symbol;

	/*! \brief Assign memory locations to sections */
	bool preload() override;

	/*! \brief Luci specific fixes performed right after mapping */
	bool fix() override;

	/*! \brief Preprepare (for tentative definitions) */
	void preprepare() override;

	/*! \brief Relocate sections */
	bool prepare() override;

	bool update() override;

	bool patchable() const override;

	Optional<VersionedSymbol> resolve_symbol(const char * name, uint32_t hash, uint32_t gnu_hash, const VersionedSymbol::Version & version) const override;
	Optional<VersionedSymbol> resolve_symbol(uintptr_t addr) const override;
	Optional<ElfSymbolHelper> resolve_internal_symbol(const SymbolHelper & needle) const override;

	void* relocate(const Elf::Relocation & reloc) const;

	bool initialize(bool preinit = false) override;

 private:
	uintptr_t offset = 0;
	bool preprepared = false;

	Vector<Elf::Array<Elf::Relocation>> relocation_tables;
	Vector<Elf::Section> init_sections;

	HashSet<ElfSymbolHelper, SymbolComparison> symbols;

	/*! \brief Fixup symbols & relocations after assigning an offset to a section */
	bool adjust_offsets(uintptr_t offset, const Elf::Section & section);
};
