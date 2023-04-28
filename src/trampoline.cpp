// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "trampoline.hpp"

#include <dlh/syscall.hpp>
#include <dlh/assert.hpp>
#include <dlh/math.hpp>
#include <dlh/page.hpp>
#include <dlh/log.hpp>

#include "object/identity.hpp"
#include "object/base.hpp"


const size_t trampoline_bytes = 16;  // Trampoline entry is 32 bytes
const size_t entries_per_block = Page::SIZE / sizeof(uintptr_t);
const size_t trampolines_block = Math::align_up(entries_per_block * trampoline_bytes, Page::SIZE);

static void undefined_trampoline() {
	LOG_ERROR << "Jumped to unassigned trampoline function!" << endl;
	assert(false);
}

bool Trampoline::index(const VersionedSymbol & sym, size_t & index) const {
	if (sym.type() != Elf::STT_FUNC && sym.type() != Elf::STT_GNU_IFUNC) {
		LOG_WARNING << "Symbol trampoline can only be used for function types!" << endl;
	} else {
		for (size_t i = 0; i < symbols.size(); i++) {
			if (symbols[i] == sym) {
				index = i;
				return true;
			}
		}
	}
	return false;
}

bool Trampoline::pointer(size_t index, void* & trampoline_function, uintptr_t* & target_address) const {
	auto block = index / entries_per_block;
	if (index < symbols.size() && block < blocks.size()) {
		uintptr_t base = blocks[block];
		size_t offset = index % entries_per_block;
		trampoline_function = reinterpret_cast<void*>(base + offset * trampoline_bytes);
		target_address = reinterpret_cast<uintptr_t*>(base + trampolines_block + offset * sizeof(uintptr_t));
		return true;
	}
	return false;
}


bool Trampoline::allocate(const VersionedSymbol & sym, size_t & index) {
	if ((symbols.size() + 1) / entries_per_block >= blocks.size()) {
		// allocate new pages
		if (auto mmap = Syscall::mmap(0, trampolines_block + Page::SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE)) {
			uint8_t * addr = reinterpret_cast<uint8_t*>(mmap.value());
			uintptr_t * entry = reinterpret_cast<uintptr_t*>(mmap.value() + trampolines_block);
			for (size_t i = 0; i < entries_per_block; i ++) {
				// Set redirection pointer to undefined
				entry[i] = reinterpret_cast<uintptr_t>(undefined_trampoline);

				// target is memory
				uint32_t target = trampolines_block              // size of trampoline code
					            + (sizeof(uintptr_t) * i)        // offset in redirection pointer memory
					            - (trampoline_bytes * i + 0xa);  // offset in trampoline code (RIP relative)

				// endbr64
				*(addr++) = 0xf3;
				*(addr++) = 0x0f;
				*(addr++) = 0x1e;
				*(addr++) = 0xfa;

				// jmp *target(rip)
				*(addr++) = 0xff;
				*(addr++) = 0x25;
				*(addr++) = (target >> 0) & 0xff;
				*(addr++) = (target >> 8) & 0xff;
				*(addr++) = (target >> 16) & 0xff;
				*(addr++) = (target >> 24) & 0xff;

				// [6 byte] nop
				*(addr++) = 0x66;
				*(addr++) = 0x0f;
				*(addr++) = 0x1f;
				*(addr++) = 0x44;
				*(addr++) = 0x00;
				*(addr++) = 0x00;
			}
			if (auto mprotect = Syscall::mprotect(mmap.value(), trampolines_block, PROT_EXEC)) {
				blocks.push_back(mmap.value());
				LOG_TRACE << "Got new pages for trampoline at " << reinterpret_cast<void*>(mmap.value()) << endl;
			} else {
				LOG_ERROR << "Protecting page at " << reinterpret_cast<void*>(mmap.value()) << " failed: " << mprotect.error_message() << endl;
				Syscall::munmap(mmap.value(), trampolines_block + Page::SIZE);
				return false;
			}
		} else {
			LOG_ERROR << "Requesting memory failed: " << mmap.error_message() << endl;
			return false;
		}
	}
	index = symbols.push_back(sym).index();
	return true;
}

void * Trampoline::get(const VersionedSymbol & sym) const {
	size_t idx = SIZE_MAX;
	if (index(sym, idx)) {
		uintptr_t * address = nullptr;
		void * trampoline = nullptr;
		if (pointer(idx, trampoline, address)) {
			return trampoline;
		}
	}
	return nullptr;
}

void * Trampoline::set(const VersionedSymbol & sym) {
	// Retrieve latest
	if (auto resolved = sym.object().file.current->resolve_symbol(sym)) {
		size_t idx = SIZE_MAX;
		if (index(sym, idx) || allocate(sym, idx)) {
			uintptr_t * address = nullptr;
			void * trampoline = nullptr;
			if (pointer(idx, trampoline, address)) {
				assert(address != nullptr);
				*address = reinterpret_cast<uintptr_t>(resolved->pointer());
				return reinterpret_cast<void*>(trampoline);
			} else {
				LOG_ERROR << "Unable to find trampoline pointer of " << idx << " for " << sym << endl;
				assert(false);
			}
		}
		LOG_ERROR << "Unable to find/create trampoline index for " << sym << endl;
	} else {
		LOG_ERROR << "Symbol " << sym << " could not be found in (current) object!" << endl;
	}
	return nullptr;
}


void Trampoline::update() {
	uintptr_t page = 0;
	for (size_t i = 0; i < symbols.size(); i++) {
		auto & sym = symbols[i];
		if (auto resolved = sym.object().file.current->resolve_symbol(sym)) {
			uintptr_t * address = nullptr;
			void * trampoline = nullptr;
			if (pointer(i, trampoline, address)) {
				assert(address != nullptr);
				*address = reinterpret_cast<uintptr_t>(resolved->pointer());
			} else {
				LOG_ERROR << "Unable to find trampoline pointer of " << i << " for " << sym << endl;
				assert(false);
			}
		} else {
			LOG_WARNING << "Symbol " << sym << " could not be found in (current) object on update!" << endl;
		}
	}
}
