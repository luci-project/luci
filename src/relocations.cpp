#include "relocations.hpp"

#include <cstdint>

typedef uint8_t word8_t;
typedef uint16_t word16_t;
typedef uint32_t word32_t;
typedef uint64_t word64_t;
typedef unsigned __int128 word64x2_t;
#ifdef __LP64__
typedef uint64_t wordclass_t;
#else // ILP32
typedef uint32_t wordclass_t;
#endif

/*! \brief Write at a specific memory address
 * \tparam T size of value
 * \param mem memory address
 * \param value value to write at memory address with given size
 * \return written value
 */
template<typename T>
static uintptr_t _write(uintptr_t mem, uintptr_t value) {
	T v = static_cast<T>(value);
	T * m = reinterpret_cast<T *>(mem);
	*m = v;
	LOG_DEBUG << "Relocation write " << reinterpret_cast<void*>(value) << " ("<< sizeof(T) << " bytes) at " << reinterpret_cast<void*>(mem);
	assert(static_cast<intptr_t>(v) == static_cast<intptr_t>(value));
	return static_cast<uintptr_t>(v);
}

template<typename T>
static uintptr_t _relocate(const T & entry, const Elf::Symbol & sym, const uintptr_t sym_base, const Object & object) {
	assert(object.elf.header.machine() == Elf::EM_X86_64);
	assert(sym.valid());

	const uintptr_t mem = object.base + entry.offset();

	const intptr_t A = entry.addend();
	const uintptr_t B = object.base;
	const uintptr_t G = sym_base + sym.value();
	const uintptr_t GOT = object.global_offset_table;
	const uintptr_t L = 0; // PLT entry of symbol
	const uintptr_t P = mem;
	const uintptr_t S = sym_base + sym.value();
	const uintptr_t Z = sym.size();

	switch (entry.type()) {
		case Elf::R_X86_64_NONE:
			return true;

		case Elf::R_X86_64_64:
			return _write<word64_t>(mem, S + A);

		case Elf::R_X86_64_PC32:
		 	return _write<word32_t>(mem, S + A - P);

		case Elf::R_X86_64_GOT32:
			return _write<word32_t>(mem, G + A);

		case Elf::R_X86_64_PLT32:
			assert(false);
			return _write<word32_t>(mem, L + A - P);

		case Elf::R_X86_64_COPY:
			assert(false);
			return true;

		case Elf::R_X86_64_GLOB_DAT:
			return _write<wordclass_t>(mem, S);

		case Elf::R_X86_64_JUMP_SLOT:
			return _write<wordclass_t>(mem, S);

		case Elf::R_X86_64_RELATIVE:
			return _write<wordclass_t>(mem, B + A);

		case Elf::R_X86_64_GOTPCREL:
			return _write<word32_t>(mem, G + GOT + A - P);

		case Elf::R_X86_64_32:
		case Elf::R_X86_64_32S:
			return _write<word32_t>(mem, S + A);

		case Elf::R_X86_64_16:
			return _write<word16_t>(mem, S + A);

		case Elf::R_X86_64_PC16:
			return _write<word16_t>(mem, S + A - P);

		case Elf::R_X86_64_8:
			return _write<word8_t>(mem, S + A);

		case Elf::R_X86_64_PC8:
			return _write<word8_t>(mem, S + A - P);
/*
		case Elf::R_X86_64_DPTMOD64:
			return _write<word64_t>();

		case Elf::R_X86_64_DTPOFF64:
			return _write<word64_t>();

		case Elf::R_X86_64_TPOFF64:
			return _write<word64_t>();

		case Elf::R_X86_64_TLSGD:
			return _write<word32_t>();

		case Elf::R_X86_64_TLSLD:
			return _write<word32_t>();

		case Elf::R_X86_64_DTPOFF32:
			return _write<word32_t>();

		case Elf::R_X86_64_GOTTPOFF:
			return _write<word32_t>();

		case Elf::R_X86_64_TPOFF32:
			return _write<word32_t>();
*/

		case Elf::R_X86_64_PC64:
			return _write<word64_t>(mem, S + A - P);

		case Elf::R_X86_64_GOTOFF64:
			return _write<word64_t>(mem, S + A - GOT);

		case Elf::R_X86_64_GOTPC32:
			return _write<word32_t>(mem, GOT + A - P);

		case Elf::R_X86_64_SIZE32:
			return _write<word32_t>(mem, Z + A);

		case Elf::R_X86_64_SIZE64:
			return _write<word64_t>(mem, Z + A);

/*
		case Elf::R_X86_64_GOTPC32_TLSDESC:
			return _write<word32_t>();

		case Elf::R_X86_64_TLSDESC_CALL:
			return true;

		case Elf::R_X86_64_TLSDESC:
			return _write<word64x2_t>();
*/
		case Elf::R_X86_64_IRELATIVE:
		{
			typedef uintptr_t (*indirect_t)();
			indirect_t func = reinterpret_cast<indirect_t>(B + A);
			return _write<wordclass_t>(mem, func());
		}

		case Elf::R_X86_64_RELATIVE64:
			return _write<word64_t>(mem, B + A);

		case Elf::R_X86_64_GOTPCRELX:
		case Elf::R_X86_64_REX_GOTPCRELX:
			return _write<word64_t>(mem, G + GOT + A - P);

		default: // Not recognized!
			assert(false);
			return 0;
	}
}

uintptr_t Relocations::relocate(const Elf::Relocation & entry, const Elf::Symbol & sym, const uintptr_t sym_base) const {
	return _relocate(entry, sym, sym_base, object);
}

uintptr_t Relocations::relocate(const Elf::RelocationWithAddend & entry, const Elf::Symbol & sym, const uintptr_t sym_base) const {
	return _relocate(entry, sym, sym_base, object);
}

size_t Relocations::size(uint32_t type) const {
	switch (object.elf.header.machine()) {
		case Elf::EM_386:
		case Elf::EM_486:
			switch (type) {
				case Elf::R_386_NONE:
				case Elf::R_386_COPY:
					return 0;

				case Elf::R_386_32:  // S + A
				case Elf::R_386_PC32:  // S + A - P
				case Elf::R_386_GOT32:  // G + A - P
				case Elf::R_386_PLT32:  // L + A - P
				case Elf::R_386_GLOB_DAT:  // S
				case Elf::R_386_JMP_SLOT:  // S
				case Elf::R_386_RELATIVE:  // B + A
				case Elf::R_386_GOTOFF:  // S + A - GOT
				case Elf::R_386_GOTPC:  // GOT + A - P
					return 4;

				default:  // Not recognized!
					assert(false);
					return 0;
			}

		case Elf::EM_X86_64:
			switch (type) {
				case Elf::R_X86_64_NONE:
				case Elf::R_X86_64_COPY:
					return 0;

				case Elf::R_X86_64_8:  // S + A
				case Elf::R_X86_64_PC8:  // S + A - P
					return 1;

				case Elf::R_X86_64_16:  // S + A
				case Elf::R_X86_64_PC16:  // S + A - P
					return 2;

				case Elf::R_X86_64_PC32:  // S + A - P
				case Elf::R_X86_64_GOT32:  // G + A
				case Elf::R_X86_64_PLT32:  // L + A - P
				case Elf::R_X86_64_GOTPCREL:  // G + GOT + A - P
				case Elf::R_X86_64_32:  // S + A
				case Elf::R_X86_64_32S:  // S + A
				case Elf::R_X86_64_TLSGD:
				case Elf::R_X86_64_TLSLD:
				case Elf::R_X86_64_DTPOFF32:
				case Elf::R_X86_64_GOTTPOFF:
				case Elf::R_X86_64_TPOFF32:
				case Elf::R_X86_64_GOTPC32:  // GOT + A - P
				//case Elf::R_X86_64_SIZE32:  // Z + A
					return 4;

				case Elf::R_X86_64_64:  // S + A
				case Elf::R_X86_64_GLOB_DAT:  // S
				case Elf::R_X86_64_JUMP_SLOT:  // S
				case Elf::R_X86_64_RELATIVE:  // B + A
				case Elf::R_X86_64_DTPMOD64:
				case Elf::R_X86_64_DTPOFF64:
				case Elf::R_X86_64_TPOFF64:
				case Elf::R_X86_64_PC64:  // S + A - P
				case Elf::R_X86_64_GOTOFF64:  // S + A - GOT
				//case Elf::R_X86_64_SIZE64:  // Z + A
					return 8;


				case Elf::R_X86_64_GOT64:  // G + A
				case Elf::R_X86_64_GOTPCREL64:  // G + GOT - P + A
				case Elf::R_X86_64_GOTPC64:  //  GOT - P + A
				case Elf::R_X86_64_GOTPLT64:  // G + A
				case Elf::R_X86_64_PLTOFF64:  // L - GOT + A
					return 8;

				default:  // Not recognized!
					assert(false);
					return 0;
			}

		default:  // unsupported architecture
			assert(false);
			return 0;
	}
}
