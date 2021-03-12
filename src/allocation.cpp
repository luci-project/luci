#include <cassert>
#include <iostream>
#include "allocation.hpp"
#include "memory.hpp"

std::vector<Allocation*> Allocation::areas;

Allocation::~Allocation() {
	if (start > 0)
		Memory::free(start, length);
}

void Allocation::load() {
	if (length > 0) {
		assert(!sections.empty());
		start = Memory::allocate(length, /* writeable */ true, executable);
		assert(start != 0);
		//const uint8_t NOP = 0x90;
		const uint8_t INT3 = 0xcc;
		Memory::clear(start, length, executable ? INT3 : 0);
		std::cerr << "load at " << start << " with "<< length <<  std::endl;
		for (auto sec : sections) {
			assert(sec.first <= length + sec.second->getSize());
			sec.second->load(start + sec.first);
		}
	}
}

void Allocation::relocate() {
	for (auto sec : sections) {
		sec.second->relocate();
	}
}

void Allocation::add(Section * sec) {
	assert(sec != nullptr);
	section* s = sec->object->elf.sections[sec->index];
	bool writeable = s->get_flags() & SHF_WRITE;
	bool executable = s->get_flags() & SHF_EXECINSTR;
	size_t size = static_cast<size_t>(s->get_size());

	if (size == 0)
		return;

	uintptr_t align = s->get_addr_align();

	// Find area
	Allocation * a = nullptr;
	for (auto area : areas){
		if (area->start == 0 && area->writeable == writeable && area->executable == executable) {
			a = area;
			break;
		}
	}
	if (a == nullptr) {
		a = new Allocation(writeable, executable);
		areas.push_back(a);
	}

	// Add section
	size_t offset = a->length;
	if (align > 1 && (offset % align) != 0)
		offset += align - (offset % align);

	a->sections[offset] = sec;
	a->length = offset + size;
}

void Allocation::loadAll() {
	for (auto area : areas)
		if (area->start == 0)
			area->load();
}

void Allocation::relocateAll() {
	for (auto area : areas)
		area->relocate();
}

void Allocation::dumpAll() {
	for (auto area : areas)
		if (area->length > 0) {
			std::cout << std::endl << "\x1b[4mMem (r";
			if (area->writeable)
				std::cout << "w";
			if (area->executable)
				std::cout << "x";
			std::cout << ") at " << (void*) area->start << " with " << area->length << " bytes\x1b[0m";

			std::map<uintptr_t, Symbol*> symlist;
			for (auto sec : area->sections)
				sec.second->getSymbols(symlist);

			std::map<uintptr_t, Relocation*> rellist;
			for (auto sec : area->sections)
				sec.second->getRelocations(rellist);

			Memory::dump(area->start, area->length, area->executable, symlist, rellist);
		}
}

void Allocation::clearAll() {
	for (auto area : areas)
		delete(area);
	areas.clear();
}
