#include <cstring>
#include <cerrno>
#include <cassert>
#include <iostream>
#include <queue>
#include <string>
#include <sys/mman.h>

#include <Zydis/Zydis.h>

#include "memory.hpp"
#include "generic.hpp"

namespace Memory {
static void disassembleHelper(ZyanU64 base, ZyanUSize length, std::map<uintptr_t, Symbol*> symbols, std::map<uintptr_t, Relocation*> relocations) {
	ZyanU8 * data = reinterpret_cast<ZyanU8*>(base);

	// Initialize decoder context
	ZydisDecoder decoder;
	ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_ADDRESS_WIDTH_64);

	// Initialize formatter. Only required when you actually plan to do instruction
	// formatting ("disassembling"), like we do here
	ZydisFormatter formatter;
	ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);
	ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_HEX_UPPERCASE, ZYAN_FALSE);
	ZydisFormatterSetProperty(&formatter, ZYDIS_FORMATTER_PROP_FORCE_RELATIVE_BRANCHES, ZYAN_TRUE);

	uintptr_t hl_end = 0;
	ZyanUSize offset = 0;
	ZydisDecodedInstruction instruction;
	while (ZYAN_SUCCESS(ZydisDecoderDecodeBuffer(&decoder, data + offset, length - offset, &instruction))) {
		char buffer[256];
		ZydisFormatterFormatInstruction(&formatter, &instruction, buffer, sizeof(buffer), base + offset);

		uintptr_t base_addr = reinterpret_cast<uintptr_t>(base + offset);
		auto entry = symbols.lower_bound(base_addr);
		if (entry != symbols.end() && entry->first < reinterpret_cast<uintptr_t>(base_addr + instruction.length)) {
			auto sym = entry->second;
			hl_end = entry->first + sym->size;
			printf("\n\x1b[%dm[%016" PRIx64 "]\x1b[0m                              \x1b[37;1m%s: \x1b[0;3m(%zu bytes, %s:%s)\x1b[0m\n", entry->first == base_addr ? 30 : 31, entry->first, sym->name.c_str(), sym->size, sym->section->object->getFileName().c_str(), sym->section->getName().c_str());
		}

		auto rel = relocations.lower_bound(base_addr);
		int rel_start = -1;
		int rel_end = -1;
		if (rel != relocations.end() && rel->first < base_addr + instruction.length) {
			rel_start = rel->first - base_addr;
		 	rel_end = rel_start + rel->second->getSize();
		}
		printf("[%016" PRIx64 "] ", base_addr);
		for (ZyanUSize i = 0 ; i < 11 ; i++) {
			if (i == rel_start && i != rel_end) {
				printf("\x1b[%d;30m", rel_end <= instruction.length ? 47 : 41);
			} else if (i == rel_end) {
				printf("\x1b[0m");
			}
			if (i < instruction.length) {
				printf("%02x ", *(reinterpret_cast<uint8_t*>(base_addr + i)));
				offset++;
			} else {
				printf("   ");
			}
		}
		printf("\x1b[0;%dm%s\x1b[0m ", base_addr < hl_end ? 37 : 0, buffer);
		if (rel_start != -1) {
			printf("\x1b[%d;30m; ", rel_end <= instruction.length ? 47 : 41);

			auto sym = rel->second->getSymbol();
			if (sym == nullptr)
				printf("(unresolved)");
			else
				printf("%s [%s:%s]", sym->name.c_str(), sym->section->object->getFileName().c_str(), sym->section->getName().c_str());
			puts("\x1b[0m");
		} else {
			puts("");
		}
	}
}

void dump(uintptr_t base, size_t length, bool disassemble, std::map<uintptr_t, Symbol*> symbols, std::map<uintptr_t, Relocation*> relocations) {
	if (disassemble) {
		printf("\n");
		disassembleHelper(static_cast<ZyanU64>(base), static_cast<ZyanUSize>(length), symbols, relocations);
		printf("\n");
	}
	else {
		uintptr_t sym_end = 0;
		uint8_t sym_color = 35;
		std::queue<std::string> names {};
		for (uintptr_t i = base - (base % 0x10); i < base + length ; i++) {
			if (i % 0x10 == 0) {
				while (!names.empty()) {
					printf("  %s", names.front().c_str());
					names.pop();
				}
				printf("\n[%016lx]", i);
			}
			if (i < base) {
				printf("   ");
				continue;
			}
			auto entry = symbols.find(i);
			if (entry != symbols.end()) {
				auto sym = entry->second;
				sym_end = i + reinterpret_cast<uintptr_t>(sym->size);
				sym_color = (sym_color % 6) + 31;
				names.push("\x1b[" + std::to_string(sym_color) + "m" + sym->name + " \x1b[3m(" + std::to_string(sym->size) + ", " + sym->section->object->getFileName() + ":" + sym->section->getName() + ")\x1b[0m");
			}
			printf(" \x1b[%um%02x\x1b[0m", i < sym_end ? sym_color : 0, *((uint8_t*)i));
		}
		for (uintptr_t i = length; i % 0x10 != 0; i++)
			printf("   ");

		while (!names.empty()) {
			printf("  %s", names.front().c_str());
			names.pop();
		}
		printf("\n");
	}
}

void dump(void * base, size_t length, bool disassemble, std::map<uintptr_t, Symbol*> symbols, std::map<uintptr_t, Relocation*> relocations) {
	dump(reinterpret_cast<uintptr_t>(base), length, disassemble, symbols, relocations);
}

uintptr_t allocate_stack(size_t stack_size) {
	errno = 0;
	void *stack = mmap(NULL, stack_size, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_STACK | MAP_ANONYMOUS, -1, 0);
	if (stack == MAP_FAILED) {
		LOG_ERROR << "Mapping Stack with " << stack_size << " Bytes failed: " << strerror(errno);
		return 0;
	} else {
		LOG_DEBUG << "Stack at " << stack << " with " << stack_size << std::endl;
		return reinterpret_cast<uintptr_t>(stack) + stack_size;
	}
}

uintptr_t allocate(size_t length, bool write, bool execute) {
	if (length == 0) {
		return 0;
	} else {
		int prot = PROT_READ;
		if (write) { prot |= PROT_WRITE; }
		if (execute) { prot |= PROT_EXEC; }
		errno = 0;
		void * base = mmap(NULL, length, prot, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
		if (base == MAP_FAILED) {
			std::cerr << "Mapping " << length << " Bytes failed: " << strerror(errno) << std::endl;
			exit(EXIT_FAILURE);
		}
		return reinterpret_cast<uintptr_t>(base);
	}
}

void free(void * base, size_t length) {
	if (length > 0 && ::munmap(base, length) != 0) {
		std::cerr << "Unmapping " << std::hex << base << " with " << std::dec << length << " Bytes failed: " << strerror(errno) << std::endl;
	}
}

void free(uintptr_t base, size_t length) {
	Memory::free(reinterpret_cast<void*>(base), length);
}

void copy(void * target, const void * source, size_t length) {
	::memcpy(target, source, length);
}

void copy(uintptr_t target, const uintptr_t source, size_t length) {
	copy(reinterpret_cast<void*>(target), reinterpret_cast<const void*>(source), length);
}

void clear(uintptr_t target, size_t length, uint8_t value) {
	clear(reinterpret_cast<void*>(target), length, value);
}

void clear(void * target, size_t length, uint8_t value) {
	::memset(target, (int)value, length);
}
}
