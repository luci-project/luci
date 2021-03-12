#include <cassert>

#include "section.hpp"
#include "memory.hpp"

size_t Section::getSize() {
	return static_cast<size_t>(object->elf.sections[index]->get_size());
}

size_t Section::load(uintptr_t addr) {
	assert(addr != 0);
	auto sec = object->elf.sections[index];
	assert(addr % sec->get_addr_align() == 0);
	size_t size = static_cast<size_t>(sec->get_size());
	if (size > 0) {
		start = addr;
		if (sec->get_type() != SHT_NOBITS) {
			uintptr_t data = reinterpret_cast<uintptr_t>(sec->get_data());
			assert(data != 0);
			Memory::copy(start, data, size);
		} else
			Memory::clear(start, size);
	}
	return size;
}

size_t Section::getSymbols(std::map<uintptr_t, Symbol*> &list) {
	size_t n = 0;
	for (auto syms : object->symbols) {
		if (syms.second->section == this && syms.second->type != STT_SECTION) {
			list[start + reinterpret_cast<uintptr_t>(syms.second->value)] = syms.second;
			n++;
		}
	}
	return n;
}

size_t Section::getRelocations(std::map<uintptr_t, Relocation *> &list) {
	size_t n = 0;
	for (auto rels : relocations) {
		assert(rels->section == this);
		list[start + reinterpret_cast<uintptr_t>(rels->offset)] = rels;
		n++;
	}
	return n;
}

std::string Section::getName() {
	return object->elf.sections[index]->get_name();
}

bool Section::relocate() {
	return ::relocate(relocations);
}
