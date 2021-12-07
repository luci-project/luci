#pragma once

#include <dlh/container/vector.hpp>

#include "versioned_symbol.hpp"

class Trampoline {
	const size_t size = 32;  // Trampoline entry is 32 bytes
	Vector<uintptr_t> addrs;
	Vector<VersionedSymbol> symbols;

	/*! \brief Get vector index of symbol */
	bool get_index(const VersionedSymbol & sym, size_t & index) const;

	/*! \brief Retrieve address of symbol in current (!) object */
	uintptr_t pointer(const VersionedSymbol & sym) const;

	/*! \brief Protected (non-writable) page at address */
	bool protect(uintptr_t page, bool writable) const;

	/*! \brief write trampoline to target at given address */
	void write_trampoline_code(void * addr, uintptr_t target) const;

	/*! \brief get corresponding address in page of trampoline code for symbol at index */
	void * get_trampoline(uintptr_t page, size_t index) const;

	/*! \brief put trampoline code for symbol at index in page to the corresponding position */
	void * set_trampoline(uintptr_t page, size_t index);

	/*! \brief Return trampoline function for symbol at index */
	void * get(size_t index) const;

	/*! \brief Generate and return trampoline code for symbol at index (including page handling) */
	void * set(size_t index);

 public:
	/*! \brief Return address of trampoline code for symbol (or nullptr if not found) */
	void * get(const VersionedSymbol & sym) const;

	/*! \brief Generate trampoline code for symbol and return its address */
	void * set(const VersionedSymbol & sym);

	/*! \brief Update all trampolines to their current objects */
	void update();
};
