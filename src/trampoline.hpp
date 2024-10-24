// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <dlh/container/vector.hpp>

#include "symbol.hpp"

class Trampoline {
	Vector<uintptr_t> blocks;  // a block starts with two code pages (similar to PLT) and an additional redirection table (GOT)
	Vector<VersionedSymbol> symbols;  // list of assigned symbols, index corresponds to entry
	uintptr_t (*address_callback)(size_t);  // callback for mmap address

	/*! \brief Get vector index of symbol */
	bool index(const VersionedSymbol & sym, size_t & index) const;

	/*! \brief Retriefe pointer to trampoline code and target address for symbol at index */
	bool pointer(size_t index, void* & trampoline_function, uintptr_t* & target_address) const;

	/*! \brief Allocate trampoline for symbol */
	bool allocate(const VersionedSymbol & sym, size_t & index);

 public:
	/*! \brief Constructor with optional callback for the address */
	explicit Trampoline(uintptr_t (*address_callback)(size_t) = nullptr) : address_callback(address_callback) {}

	/*! \brief Return address of trampoline code for symbol (or nullptr if not found) */
	void * get(const VersionedSymbol & sym) const;

	/*! \brief Generate trampoline code for symbol and return its address */
	void * set(const VersionedSymbol & sym);

	/*! \brief Update all trampolines to their current objects */
	void update();
};
