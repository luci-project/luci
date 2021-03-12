#pragma once

#include <string>
#include <vector>
#include <elfio/elfio.hpp>

struct Section;
struct Relocation;

#include "section.hpp"
#include "relocation.hpp"

using namespace ELFIO;

struct Symbol {
	std::string   name;
	Elf64_Addr    value;
	Elf_Xword     size;
	unsigned char bind;
	unsigned char type;
	unsigned char other;
	Section * section;

	std::vector<Relocation*> relocations;

	bool relocate();

	std::string hash();

	uint64_t execute(uint64_t a = 0, uint64_t b = 0, uint64_t c = 0, uint64_t d = 0, uint64_t e = 0, uint64_t f = 0);
};
