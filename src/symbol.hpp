// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <dlh/container/optional.hpp>
#include <dlh/stream/buffer.hpp>
#include <dlh/assert.hpp>
#include <dlh/string.hpp>

#include <elfo/elf.hpp>

struct Object;

// For Hash Set/Map
struct SymbolHelper {
	SymbolHelper(const char * name, uint32_t gnu_hash) : _name(name), _gnu_hash_value(gnu_hash) {}
	SymbolHelper(const char * name) : SymbolHelper(name, ELF_Def::gnuhash(name)) {}  // NOLINT
	SymbolHelper(const SymbolHelper & other) = default;
	SymbolHelper(SymbolHelper && other) = default;

	const char * name() const {
		return _name;
	}

	uint32_t gnu_hash_value() const {
		return _gnu_hash_value;
	}

	template <typename T>
	bool operator==(const T & o) const {
		return gnu_hash_value() == o.gnu_hash_value() && (name() == o.name() || String::compare(name(), o.name()) == 0);
	}

	template <typename T>
	bool operator!=(const T & o) const {
		return !operator==(o);
	}

 private:
	const char * _name;
	const uint32_t _gnu_hash_value;
};

struct ElfSymbolHelper : Elf::Symbol {
	using Elf::Symbol::name;
	using Elf::Symbol::value;
	using Elf::Symbol::size;
	using Elf::Symbol::bind;
	using Elf::Symbol::type;
	using Elf::Symbol::visibility;
	using Elf::Symbol::section_index;

	ElfSymbolHelper(const Elf::Symbol & sym) : Elf::Symbol(sym), _gnu_hash_value(ELF_Def::gnuhash(sym.name())) {}  // NOLINT
	ElfSymbolHelper(const ElfSymbolHelper & other) = default;
	ElfSymbolHelper(ElfSymbolHelper && other) = default;

	uint32_t gnu_hash_value() const {
		return _gnu_hash_value;
	}

	template <typename T>
	bool operator==(const T & o) const {
		return gnu_hash_value() == o.gnu_hash_value() && (name() == o.name() || String::compare(name(), o.name()) == 0);
	}

	template <typename T>
	bool operator!=(const T & o) const {
		return !operator==(o);
	}

 private:
	const uint32_t _gnu_hash_value;
};

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
		const char * file;
		uint32_t hash, filehash;
		bool valid, weak;

		bool operator==(const Version & that) const {
			return this->valid && that.valid
			    && this->hash == that.hash
			    && (this->name == that.name || String::compare(this->name, that.name) == 0)
			    && (this->file == nullptr || that.file == nullptr || this->file == that.file || (this->filehash == that.filehash && String::compare(this->file, that.file) == 0));
		}

		Version(const char * name, uint32_t hash, bool weak, const char * file, uint32_t filehash) : name(name), file(file), hash(hash), filehash(file == nullptr ? 0 : filehash), valid(true), weak(weak) {}
		Version(const char * name, uint32_t hash, bool weak, const char * file) : Version(name, hash, weak, file, file == nullptr ? 0 : ELF_Def::hash(file)) {}
		Version(const char * name, uint32_t hash, bool weak = false) : Version(name, hash, weak, nullptr) {}
		Version(const char * name, bool weak = false, const char * file = nullptr) : Version(name, ELF_Def::hash(name), weak, file) {}
		Version(bool valid = true) : name(nullptr), file(nullptr), hash(0), filehash(0), valid(valid), weak(false) {}
	} version;

	VersionedSymbol(const Elf::Symbol & sym, const char * version_name = nullptr, bool version_weak = false, const char * version_file = nullptr);  // NOLINT
	VersionedSymbol(const Elf::Symbol & sym, const Version & version, uint32_t hash, uint32_t gnu_hash);
	VersionedSymbol(const Elf::Symbol & sym, const Version & version);
	VersionedSymbol(const VersionedSymbol & other) = default;
	VersionedSymbol(VersionedSymbol && other) = default;

	bool operator==(const SymbolHelper & o) const {
		return gnu_hash_value() == o.gnu_hash_value() && (name() == o.name() || String::compare(name(), o.name()) == 0);
	}

	bool operator==(const ElfSymbolHelper & o) const {
		return gnu_hash_value() == o.gnu_hash_value() && (name() == o.name() || String::compare(name(), o.name()) == 0);
	}

	bool operator==(const VersionedSymbol & o) const;

	template <typename T>
	bool operator!=(const T & o) const {
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

	/*! \brief Object this symbol belongs to */
	const Object & object() const;

	/*! \brief Pointer to target (resolve ifunc) */
	void * pointer() const;

 private:
	mutable Optional<uint32_t> _hash_value;
	mutable Optional<uint32_t> _gnu_hash_value;
};

struct SymbolComparison {
	template <typename T, typename O>
	static inline bool equal(const T & a, const O & b) {
		return a.operator==(b);
	}

	template <typename T>
	static inline uint32_t hash(const T & v) {
		return v.gnu_hash_value();
	}
};


BufferStream& operator<<(BufferStream& bs, const VersionedSymbol & s);
BufferStream& operator<<(BufferStream& bs, const Optional<VersionedSymbol> & s);
