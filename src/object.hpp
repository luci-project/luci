#pragma once

#include <string>
#include <unordered_map>
#include <elfio/elfio.hpp>

using namespace ELFIO;

struct Section;
struct Symbol;
#include "section.hpp"
#include "symbol.hpp"

struct Object {
	std::string path;
	elfio elf;
	uintptr_t base = 0;
	std::unordered_map<std::string, Elf_Xword> undefSymbols;
	std::unordered_map<Elf_Xword, Symbol*> symbols;
	std::unordered_map<Elf_Half, Section*> sections;

	Object(const std::string & path);

	std::string getFileName();
};
