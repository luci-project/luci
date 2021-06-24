#pragma once

#include <dlh/container/optional.hpp>
#include <dlh/stream/buffer.hpp>
#include <dlh/assert.hpp>
#include <dlh/string.hpp>

#include <elfo/elf.hpp>

struct Object;

/*! \brief Symbol with version */
struct VersionedSymbol : Elf::Symbol {
	using Elf::Symbol::valid;
	using Elf::Symbol::name;
	using Elf::Symbol::value;
	using Elf::Symbol::size;
	using Elf::Symbol::bind;
	using Elf::Symbol::type;
	using Elf::Symbol::elf;

	const struct Version {
		const char * name;
		uint32_t hash;
		bool valid, weak;

		bool operator==(const Version & that) const {
			return this->valid && that.valid && this->hash == that.hash && (this->name == that.name || strcmp(this->name, that.name) == 0);
		}

		Version(const char * name, uint32_t hash, bool weak = false) : name(name), hash(hash), valid(true), weak(weak) {}

		Version(const char * name, bool weak = false) : Version(name, ELF_Def::hash(name), weak) {}

		Version(bool valid = true) : name(nullptr), hash(0), valid(valid), weak(false) {}
	} version;

	VersionedSymbol(const Elf::Symbol & sym, const char * version_name = nullptr, bool version_weak = false);

	VersionedSymbol(const Elf::Symbol & sym, const Version & version);

	bool operator==(const VersionedSymbol & o) const;

	bool operator!=(const VersionedSymbol & o) const {
		return !operator==(o);
	}

	uint32_t hash_value() const {
		if (!_hash_value.has_value())
			_hash_value = ELF_Def::hash(this->name());
		return _hash_value.value();
	}

	uint32_t gnu_hash_value() const {
		if (!_gnu_hash_value.has_value())
			_gnu_hash_value = ELF_Def::gnuhash(this->name());
		return _gnu_hash_value.value();
	}

	const Object & object() const;

 private:
	mutable Optional<uint32_t> _hash_value;
	mutable Optional<uint32_t> _gnu_hash_value;
};

BufferStream& operator<<(BufferStream& bs, const VersionedSymbol & s);
BufferStream& operator<<(BufferStream& bs, const Optional<VersionedSymbol> & s);
