#pragma once

#include <inttypes.h>
#include <vector>
#include <map>
#include <string>
#include <elfio/elfio.hpp>

struct Object;
struct Symbol;
struct Relocation;

#include "object.hpp"
#include "symbol.hpp"
#include "relocation.hpp"

using namespace ELFIO;

struct Section {
	Elf_Half index;
	Object * object;
	uintptr_t start;

	std::vector<Relocation*> relocations;

	Section(Object * object, Elf_Half index) : index(index), object(object), start(0) {}

	size_t load(uintptr_t addr);

	size_t getSymbols(std::map<uintptr_t, Symbol*> &list);

	size_t getRelocations(std::map<uintptr_t, Relocation*> &list);

	size_t getSize();

	std::string getName();

	bool relocate();
};
