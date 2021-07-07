#pragma once

#include <elfo/elf.hpp>

struct Patch {
	const char * symname;
	uint32_t hash;
	uint32_t gnuhash;
	uintptr_t replace;

	Patch(const char * symname, uintptr_t replace)
	  : symname(symname), hash(ELF_Def::hash(symname)), gnuhash(ELF_Def::gnuhash(symname)), replace(replace) {}

	bool apply(const Elf::SymbolTable & symtab, uintptr_t base = 0) const;
};
