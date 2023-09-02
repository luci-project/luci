// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <elfo/elf.hpp>
#include <bean/bean.hpp>

#include "object/base.hpp"

namespace BeanInterface {

struct Symbol {
	const Bean::Symbol & _sym;

	/*! \brief Constructor */
	explicit Symbol(const Bean::Symbol & sym) : _sym(sym) {}

	/*! \brief Symbol name */
	const char * name() const {
		return _sym.name;
	}

	/*! \brief Symbol value */
	uintptr_t value() const {
		return _sym.address;
	}

	/*! \brief Size of Symbol */
	size_t size() const {
		return _sym.size;
	}

	/*! \brief Is the symbol valid? */
	bool valid() const {
		return value() != 0 || size() != 0;
	}

	/*! \brief Is the symbol undefined (extern)? */
	bool undefined() const {
		return false;
	}

	/*! \brief Symbol binding */
	Elf::sym_bind bind() const {
		switch(_sym.bind) {
			case Bean::Symbol::BIND_WEAK:
				return Elf::STB_WEAK;
			case Bean::Symbol::BIND_LOCAL:
				return Elf::STB_LOCAL;
			case Bean::Symbol::BIND_GLOBAL:
				return Elf::STB_GLOBAL;
			default:
				assert(false);
				return Elf::STB_HIPROC;
		}
	}

	/*! \brief Symbol type */
	Elf::sym_type type() const {
		switch (_sym.type) {
			case Bean::Symbol::TYPE_NONE:
			case Bean::Symbol::TYPE_UNKNOWN:
				return Elf::STT_NOTYPE;
			case Bean::Symbol::TYPE_OBJECT:
				return Elf::STT_OBJECT;
			case Bean::Symbol::TYPE_FUNC:
				return Elf::STT_FUNC;
			case Bean::Symbol::TYPE_SECTION:
				return Elf::STT_SECTION;
			case Bean::Symbol::TYPE_FILE:
				return Elf::STT_FILE;
			case Bean::Symbol::TYPE_COMMON:
				return Elf::STT_COMMON;
			case Bean::Symbol::TYPE_TLS:
				return Elf::STT_TLS;
			case Bean::Symbol::TYPE_INDIRECT_FUNC:
				return Elf::STT_GNU_IFUNC;
			default:
				assert(false);
				return Elf::STT_HIPROC;
		}
	}
};

struct Relocation {
	const Elf & _elf;
	const Bean::SymbolRelocation & _rel;

	/*! \brief Constructor */
	Relocation(const Object & obj, const Bean::SymbolRelocation & rel) : _elf(obj), _rel(rel) {}

	/*! \brief Elf object */
	const Elf & elf() const {
		return _elf;
	}

	/*! \brief Valid relocation */
	bool valid() const {
		return true;
	}

	/*! \brief Address */
	uintptr_t offset() const {
		return _rel.offset;
	}

	/*! \brief Relocation type (depends on architecture) */
	uint32_t type() const {
		return _rel.type;
	}

	/*! \brief Addend */
	intptr_t addend() const {
		return _rel.addend;
	}
};

}  // namespace BeanInterface
