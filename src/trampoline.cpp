#include "trampoline.hpp"

#include <dlh/syscall.hpp>
#include <dlh/assert.hpp>
#include <dlh/log.hpp>

#include "object/identity.hpp"
#include "object/base.hpp"
#include "page.hpp"

bool Trampoline::get_index(const VersionedSymbol & sym, size_t & index) const {
	for (size_t i = 0; i < symbols.size(); i++) {
		auto & other = symbols[i];
		if (&(sym.object().file) == &(other.object().file) && (sym.name() == other.name() || strcmp(sym.name(), other.name()) == 0) && sym.version == other.version) {
			index = i;
			return true;
		}
	}
	return false;
}


uintptr_t Trampoline::pointer(const VersionedSymbol & sym) const {
	uintptr_t target = 0;
	// Retrieve latest
	if (auto resolved = sym.object().file.current->resolve_symbol(sym)) {
		target = reinterpret_cast<uintptr_t>(resolved->pointer());
	} else {
		LOG_WARNING << "Symbol " << sym << " could not be found in (current) object!" << endl;
	}
	return target;
}


bool Trampoline::protect(uintptr_t page, bool writable) const {
	if (page == 0)
		return false;

	// Protect mapping
	int protection = PROT_READ | PROT_EXEC;
	if (writable)
		protection |= PROT_WRITE;

	if (auto mprotect = Syscall::mprotect(page, Page::SIZE, protection)) {
		return true;
	} else {
		LOG_ERROR << "Protecting page at " << (void*)page << " failed: " << mprotect.error_message() << endl;
		return false;
	}
}


void Trampoline::write_trampoline_code(void* addr, uintptr_t target) const {
	uint8_t * a = reinterpret_cast<uint8_t*>(addr);

	// endbr64
	*(a++) = 0xf3;
	*(a++) = 0x0f;
	*(a++) = 0x1e;
	*(a++) = 0xfa;
	// movabs $target, %r11
	*(a++) = 0x49;
	*(a++) = 0xbb;
	*(a++) = (target >> 0) & 0xff;
	*(a++) = (target >> 8) & 0xff;
	*(a++) = (target >> 16) & 0xff;
	*(a++) = (target >> 24) & 0xff;
	*(a++) = (target >> 32) & 0xff;
	*(a++) = (target >> 40) & 0xff;
	*(a++) = (target >> 48) & 0xff;
	*(a++) = (target >> 56) & 0xff;
	// jmpq *%r11
	*(a++) = 0x41;
	*(a++) = 0xff;
	*(a++) = 0xe3;
	// fill with nops
	while (reinterpret_cast<uintptr_t>(a) % 16 != 0) {
		*(a++) = 0x90;
	}

	assert(reinterpret_cast<uintptr_t>(a) - reinterpret_cast<uintptr_t>(addr) == size);
}


void * Trampoline::get_trampoline(uintptr_t page, size_t index) const {
	return reinterpret_cast<void*>(page + ((index * size) % Page::SIZE));
}


void * Trampoline::set_trampoline(uintptr_t page, size_t index) {
	// get target
	uintptr_t target = pointer(symbols[index]);
	if (target == 0)
		return nullptr;

	// Write trampoline
	void* addr = get_trampoline(page, index);
	write_trampoline_code(addr, target);

	// Pointer to trampoline
	return addr;
}


void * Trampoline::get(size_t index) const {
	assert(index < symbols.size());
	size_t addrs_index = index / (Page::SIZE / size);
	assert(addrs_index <= addrs.size());
	return get_trampoline(addrs[addrs_index], index);
}


void * Trampoline::set(size_t index) {
	assert(index < symbols.size());

	size_t addrs_index = index / (Page::SIZE / size);
	assert(addrs_index <= addrs.size());

	uintptr_t page;
	if (addrs_index == addrs.size()) {
		// Map new page
		if (auto mmap = Syscall::mmap(0, Page::SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE)) {
			page = mmap.value();
			addrs.push_back(page);
			LOG_TRACE << "Got new page for trampoline at " << (void*)mmap.value() << endl;
		} else {
			LOG_ERROR << "Requesting memory failed: " << mmap.error_message() << endl;
			return nullptr;
		}
	} else {
		page = addrs[addrs_index];
		if (!protect(page, true))
			return nullptr;
	}

	// create/overwrite trampoline
	void * addr = set_trampoline(page, index);

	// Protect mapping
	protect(page, false);

	return addr;
}


void * Trampoline::get(const VersionedSymbol & sym) const {
	if (sym.type() != Elf::STT_FUNC && sym.type() != Elf::STT_GNU_IFUNC) {
		LOG_WARNING << "Symbol trampoline can only be used for function types!" << endl;
		return nullptr;
	}

	size_t index = symbols.size();
	if (get_index(sym, index))
		return get(index);

	return nullptr;
}


void * Trampoline::set(const VersionedSymbol & sym) {
	if (sym.type() != Elf::STT_FUNC && sym.type() != Elf::STT_GNU_IFUNC) {
		LOG_WARNING << "Symbol trampoline can only be used for function types!" << endl;
		return nullptr;
	}

	size_t index = symbols.size();
	if (!get_index(sym, index))
		symbols.push_back(sym);

	return set(index);
}


void Trampoline::update() {
	uintptr_t page = 0;
	const size_t entries = Page::SIZE / size;
	for (size_t i = 0; i < symbols.size(); i++) {
		if (i % entries == 0) {
			// protect old page
			protect(page, false);
			// get current page
			size_t addrs_index = i / entries;
			assert(addrs_index <= addrs.size());
			page = addrs[addrs_index];
			// unprotect current page
			protect(page, false);
		}
		set_trampoline(page, i);
	}
	// protect old page
	protect(page, false);
}
