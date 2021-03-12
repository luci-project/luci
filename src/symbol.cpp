#include "symbol.hpp"
#include <sstream>
#include <cassert>

bool Symbol::relocate() {
	return ::relocate(relocations);
}

uint64_t Symbol::execute(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
	uint64_t (*func)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) = (uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)) (section->start + value);
	return func(a, b, c, d, e, f);
}

std::string Symbol::hash() {
	auto sec = section->object->elf.sections[section->index];

	std::stringstream ss;
	if (size > 0 && sec->get_type() != SHT_NOBITS) {
		uintptr_t data = reinterpret_cast<uintptr_t>(sec->get_data());
		// TODO: if alloc in MEM
		for (uintptr_t off = value; off < value + size; off++){
			bool isReloc = false;
			for (auto & rel : section->relocations) {
				if (off == rel->offset) {
					auto size = rel->getSize();
					off += size - 1;
					isReloc = true;
					ss << "[" << rel->getSymbol()->name << " Type " << rel->type << " addend " << rel->addend << " size " << rel->getSize() << "]";
					break;
				}
			}
			if (!isReloc) {
				ss << std::hex << (int)*reinterpret_cast<uint8_t*>(data + off) << " ";
			}
		}

	}
	return ss.str();
}
