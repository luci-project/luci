#pragma once

#include <string>
#include <vector>
#include <elfio/elfio.hpp>

struct Section;
struct Symbol;

#include "section.hpp"
#include "symbol.hpp"

using namespace ELFIO;


struct Relocation {
	Elf64_Addr offset;
	Elf_Word symbol;
	Elf_Word type;
	Elf_Sxword addend;

	Section * section;

	Relocation(Section * section) : section(section) {}

	Symbol * getSymbol();

	size_t getSize();

	bool relocate();
};

bool relocate(std::vector<Relocation*> rels);
