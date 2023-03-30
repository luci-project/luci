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

#include "symbol.hpp"
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

	/*! \brief Begin offset of virtual memory area (for position independent objects only) */
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

	/*! \brief lookup paths */
	Vector<const char *> rpath, runpath;

	/*! \brief Segments to be loaded in memory */
	mutable Vector<MemorySegment> memory_map;

	/*! \brief Build ID, if available (null terminated) */
	char build_id[41];

	/*! \brief Mapping protected? */
	bool mapping_protected = false;

	/*! \brief Binary symbol hashes */
	Optional<Bean> binary_hash;

	/*! \brief DWARF hash contents (if enabled) */
	const char * debug_hash = nullptr;

	/*! \brief Relocations to external symbols used in this object (cache) */
	mutable HashMap<Elf::Relocation, VersionedSymbol> relocations;

	/*! \brief Pointer to previous version */
	Object * file_previous = nullptr;

	/*! \brief create new object */
	Object(ObjectIdentity & file, const Data & data);

	Object(const Object&) = delete;
	Object& operator=(const Object&) = delete;

	/*! \brief destroy object */
	virtual ~Object();

	/*! \brief query debug hash */
	const char * query_debug_hash();

	/*! \brief Get address of dynamic section */
	uintptr_t dynamic_address() const;

	/*! \brief check if this object is the current (latest) version */
	bool is_latest_version() const;

	/*! \brief virtual memory range used by this object */
	bool memory_range(uintptr_t & start, uintptr_t & end) const;

	/*! \brief resolve dynamic relocation entry (if possible!) */
	virtual void* dynamic_resolve(size_t index) const;

	/*! \brief Luci specific fixes performed right after mapping */
	virtual bool fix() { return true; };

	/*! \brief Initialisation of object (after creation) */
	virtual bool preload() = 0;

	/*! \brief allocate required segments in memory */
	bool map();

	/*! \brief Prepare relocations */
	virtual bool prepare() { return true; };

	/*! \brief Update relocations */
	virtual bool update() { return true; };

	/*! \brief Does this object use memory aliasing for data? */
	virtual bool use_data_alias() const { return false; };

	/*! \brief Helper to get pointer in compose buffer corresponding to an active address */
	template<typename T>
	T * compose_pointer(T * pointer) {
		auto ptr = reinterpret_cast<uintptr_t>(pointer);
		for (auto & seg : memory_map)
			if (seg.target.contains(ptr))
				return reinterpret_cast<T *>(seg.compose() + ptr - seg.target.address());
		return nullptr;
	}

	/*! \brief Update mapping and set protection in memory */
	virtual bool finalize() const;

	/*! \brief Initialisation method */
	virtual bool initialize(bool preinit = false) { (void)preinit; return true; };

	/*! \brief Check if current object can patch a previous version */
	virtual bool patchable() const { return false; };

	/*! \brief Make this (old) object inactive */
	virtual bool disable() const;

	/*! \brief Get (internal) version = number of updates */
	size_t version() const;

	/*! \brief Get TLS address (for current thread) */
	uintptr_t tls_address(uintptr_t value) const;

	/*! \brief Find (even internal) symbols in this object with same name */
	virtual Optional<ElfSymbolHelper> resolve_internal_symbol(const SymbolHelper & sym) const {
		(void) sym;
		return {};
	}

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
