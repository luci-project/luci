#include <cassert>
#include <iostream>
#include "relocation.hpp"

Symbol * Relocation::getSymbol() {
	return section->object->symbols[symbol];

}


template<typename T>
bool write(uintptr_t mem, uintptr_t value) {
	T v = static_cast<T>(value);
	T * m = reinterpret_cast<T *>(mem);
	*m = v;
	std::cerr << "write " << reinterpret_cast<void*>(value) << " ("<< sizeof(T) << " bytes) at " << reinterpret_cast<void*>(mem) << std::endl;
	return static_cast<intptr_t>(v) == static_cast<intptr_t>(value);
}

bool Relocation::relocate() {
	assert(section->object != nullptr);
	assert(section->object->elf.get_machine() == EM_X86_64);
	auto sym = getSymbol();
	assert(sym != nullptr);

	uintptr_t mem = section->start + offset;

	const Elf_Sxword A = addend;
	const uintptr_t B = section->start;
	const uintptr_t G = sym->section->start + sym->value;
	const uintptr_t GOT = 0;
	const uintptr_t L = 0;
	const Elf64_Addr P = mem;
	const Elf64_Addr S = sym->section->start + sym->value;
	const Elf64_Addr Z = sym->size;

	switch (type & 31) {
		case R_X86_64_NONE:
			return true;

		case R_X86_64_64:
			return write<uint64_t>(mem, S + A);

		case R_X86_64_PC32:
		 	return write<uint32_t>(mem, S + A - P);

		case R_X86_64_GOT32:
			return write<uint32_t>(mem, G + A);

		case R_X86_64_PLT32:
			return write<uint32_t>(mem, L + A - P);

		case R_X86_64_COPY:
			return true;

		case R_X86_64_GLOB_DAT:
			return write<uint64_t>(mem, S);

		case R_X86_64_JUMP_SLOT:
			return write<uint64_t>(mem, S);

		case R_X86_64_RELATIVE:
			return write<uint64_t>(mem, B + A);

		case R_X86_64_GOTPCREL:
			return write<uint32_t>(mem, G + GOT + A - P);

		case R_X86_64_32:
			return write<uint32_t>(mem, S + A);

		case R_X86_64_32S:
			return write<uint32_t>(mem, S + A);

		case R_X86_64_16:
			return write<uint16_t>(mem, S + A);

		case R_X86_64_PC16:
			return write<uint16_t>(mem, S + A - P);

		case R_X86_64_8:
			return write<uint8_t>(mem, S + A);

		case R_X86_64_PC8:
			return write<uint8_t>(mem, S + A - P);
/*
		case R_X86_64_DPTMOD64 16 word64
		case R_X86_64_DTPOFF64 17 word64
		case R_X86_64_TPOFF64 18 word64
		case R_X86_64_TLSGD 19 word32
		case R_X86_64_TLSLD 20 word32
		case R_X86_64_DTPOFF32 21 word32
		case R_X86_64_GOTTPOFF 22 word32
		case R_X86_64_TPOFF32 23 word32
*/

		case R_X86_64_PC64:
			return write<uint64_t>(mem, S + A - P);

		case R_X86_64_GOTOFF64:
			return write<uint64_t>(mem, S + A - GOT);

		case R_X86_64_GOTPC32:
			return write<uint32_t>(mem, GOT + A - P);

		default: // Not recognized symbol!
			assert(false);
			return 0;
	}
}

bool relocate(std::vector<Relocation*> rels) {
	bool success = true;
	for (auto rel: rels) {
		rel->relocate();
		 {
			std::cerr << "Relocation of '" << rel->getSymbol()->name << "'"
			          << " in " << rel->section->object->getFileName() << ": " << rel->section->getName()
			          << " at " << reinterpret_cast<void*>(rel->section->start + rel->offset)
					  << " with type = " << rel->type
					  << ", offset = " << rel->offset
					  << ", addend = " << rel->addend
					  << " failed!" << std::endl;
			success = false;
		}
	}
	return success;
}

size_t Relocation::getSize() {
	switch (section->object->elf.get_machine()) {
		case EM_386:
			switch (type) {
				case R_386_NONE:
				case R_386_COPY:
					return 0;

				case R_386_32: // S + A
				case R_386_PC32: // S + A - P
				case R_386_GOT32: // G + A - P
				case R_386_PLT32: // L + A - P
				case R_386_GLOB_DAT: // S
				case R_386_JMP_SLOT: // S
				case R_386_RELATIVE: // B + A
				case R_386_GOTOFF: // S + A - GOT
				case R_386_GOTPC: // GOT + A - P
					return 4;

				default: // Not recognized symbol!
					assert(false);
					return 0;
			}

		case EM_X86_64:
			switch (type & 31) {
				case R_X86_64_NONE:
				case R_X86_64_COPY:
					return 0;

				case R_X86_64_8: // S + A
				case R_X86_64_PC8: // S + A - P
					return 1;

				case R_X86_64_16: // S + A
				case R_X86_64_PC16: // S + A - P
					return 2;

				case R_X86_64_PC32: // S + A - P
				case R_X86_64_GOT32: // G + A
				case R_X86_64_PLT32: // L + A - P
				case R_X86_64_GOTPCREL: // G + GOT + A - P
				case R_X86_64_32: // S + A
				case R_X86_64_32S: // S + A
				case R_X86_64_TLSGD:
				case R_X86_64_TLSLD:
				case R_X86_64_DTPOFF32:
				case R_X86_64_GOTTPOFF:
				case R_X86_64_TPOFF32:
				case R_X86_64_GOTPC32: // GOT + A - P
				//case R_X86_64_SIZE32: // Z + A
					return 4;

				case R_X86_64_64: // S + A
				case R_X86_64_GLOB_DAT: // S
				case R_X86_64_JUMP_SLOT: // S
				case R_X86_64_RELATIVE: // B + A
				case R_X86_64_DTPMOD64:
				case R_X86_64_DTPOFF64:
				case R_X86_64_TPOFF64:
				case R_X86_64_PC64: // S + A - P
				case R_X86_64_GOTOFF64: // S + A - GOT
				//case R_X86_64_SIZE64: // Z + A
					return 8;


				case R_X86_64_GOT64: // G + A
				case R_X86_64_GOTPCREL64: // G + GOT - P + A
				case R_X86_64_GOTPC64: //  GOT - P + A
				case R_X86_64_GOTPLT64: // G + A
				case R_X86_64_PLTOFF64: // L - GOT + A
					return 8;

				default: // Not recognized symbol!
					assert(false);
					return 0;
			}

		default: // unsupported architecture
			assert(false);
			return 0;
	}
}
