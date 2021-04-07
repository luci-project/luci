#pragma once

#include <string>

#include <elf.hpp>

#include "generic.hpp"

class Object;

struct Symbol : Elf::Symbol {
	using Elf::Symbol::valid;
	using Elf::Symbol::name;
	using Elf::Symbol::value;
	using Elf::Symbol::size;
	using Elf::Symbol::bind;
	using Elf::Symbol::type;

	const Object & object;

	struct Version {
		const char * const name;
		const uint32_t hash;
		bool valid, weak;

		bool operator==(const Version & that) const {
			return this->valid && that.valid && this->hash == that.hash && (this->name == that.name || strcmp(this->name, that.name) == 0);
		}

		Version(const char * name, uint32_t hash, bool weak = false) : name(name), hash(hash), valid(true), weak(weak) {}

		Version(const char * name, bool weak = false) : Version(name, ELF_Def::hash(name), weak) {}

		Version(bool valid = true) : name(nullptr), hash(0), valid(valid), weak(false) {}
	} version;

	Symbol(const Object & object, const Elf::Symbol & sym, const char * version_name = nullptr, bool version_weak = false);

	Symbol(const Object & object, const Elf::Symbol & sym, const Version & version);

	Symbol(const Object & object);

	bool operator==(const Symbol & o) const {
		return this->_data == o._data || (&object == &o.object && (name() == o.name() || strcmp(name(), o.name()) == 0) && version == o.version);
	}

	bool operator!=(const Symbol & o) const {
		return !operator==(o);
	}
};

std::ostream& operator<<(std::ostream& os, const Symbol & s);
