#pragma once

#include <dlh/assert.hpp>
#include <dlh/string.hpp>
#include <dlh/types.hpp>
#include <dlh/utility.hpp>
#include <dlh/container/vector.hpp>
#include <dlh/container/pair.hpp>
#include <dlh/container/optional.hpp>
#include <dlh/stream/buffer.hpp>
#include <dlh/stream/output.hpp>
#include <dlh/strptr.hpp>

#include <elfo/elf.hpp>
#include <bean/bean.hpp>

#include "versioned_symbol.hpp"
#include "memory_segment.hpp"

struct ObjectIdentity;

struct Object : public Elf {
	/*! \brief Information about the object file (shared by all versions) */
	ObjectIdentity & file;

	/*! \brief Data (version specific) */
	const struct Data {
		/*! \brief last modification time */
		struct timespec modification_time = { 0, 0 };

		/*! \brief File size */
		size_t size = 0;

		/*! \brief File descriptor for this object */
		int fd = -1;

		/*! \brief Address of data in memory */
		uintptr_t addr = 0;

		/*! \brief File data hash */
		uint64_t hash = 0;
	} data;

	/*! \brief Begin offset of virtual memory area (for dynamic objects) */
	uintptr_t base = 0;

	/*! \brief File relative address of global offset table (for dynamic objects) */
	uintptr_t global_offset_table = 0;

	/*! \brief preparation status */
	enum {
		STATUS_MAPPED,
		STATUS_PREPARING,
		STATUS_PREPARED
	} status = STATUS_MAPPED;

	/*! \brief Symbol dependencies to other objects */
	Vector<ObjectIdentity *> dependencies;

	/*! \brief Segments to be loaded in memory */
	Vector<MemorySegment> memory_map;

	/*! \brief Binary symbol hashes */
	Optional<Bean> binary_hash;

	/*! \brief Relocations to external symbols used in this object (cache) */
	mutable Vector<Pair<Elf::Relocation, VersionedSymbol>> relocations;

	/*! \brief Pointer to previous version */
	Object * file_previous = nullptr;

	/*! \brief create new object */
	Object(ObjectIdentity & file, const Data & data);

	Object(const Object&) = delete;
	Object& operator=(const Object&) = delete;

	/*! \brief destroy object */
	virtual ~Object();

	/*! check if this object is the current (latest) version */
	bool is_latest_version() const;

	/*! \brief virtual memory range used by this object */
	bool memory_range(uintptr_t & start, uintptr_t & end) const;

	/*! \brief resolve dynamic relocation entry (if possible!) */
	virtual void* dynamic_resolve(size_t index) const;

	/*! \brief Initialisation of object (after creation) */
	virtual bool preload() = 0;

	/*! \brief allocate required segments in memory */
	bool map();

	/*! \brief Prepare relocations */
	virtual bool prepare() { return true; };

	/*! \brief Update relocations */
	virtual bool update() { return true; };

	/*! \brief Set protection flags in memory */
	bool protect();

	/*! \brief Initialisation method */
	virtual bool initialize() { return true; };

	/*! \brief Check if current object can patch a previous version */
	virtual bool patchable() const { return false; };

	/*! \brief Find (external visible) symbol in this object with same name and version */
	Optional<VersionedSymbol> resolve_symbol(const VersionedSymbol & sym) const {
		return resolve_symbol(sym.name(), sym.hash_value(), sym.gnu_hash_value(), sym.version);
	}
	Optional<VersionedSymbol> resolve_symbol(const char * name, const char * version = nullptr) const {
		return resolve_symbol(name, ELF_Def::hash(name), ELF_Def::gnuhash(name), VersionedSymbol::Version(version));
	}
	virtual Optional<VersionedSymbol> resolve_symbol(const char * name, uint32_t hash, uint32_t gnu_hash, const VersionedSymbol::Version & version) const {
		(void) name;
		(void) hash;
		(void) gnu_hash;
		(void) version;
		return {};
	};

	/*! \brief Find (external visible) symbol in this object overlapping the given address */
	virtual Optional<VersionedSymbol> resolve_symbol(uintptr_t addr) const {
		(void) addr;
		return {};
	};

	/*! \brief Check & get the symbol */
	bool has_symbol(const char * name, uint32_t hash, uint32_t gnu_hash, const VersionedSymbol::Version & version, Optional<VersionedSymbol> & result) const;

	bool operator==(const Object & o) const {
		return this == &o;
	}

	bool operator!=(const Object & o) const {
		return !operator==(o);
	}
};
