#pragma once

#include <utility>
#include <vector>
#include <unordered_map>
#include <map>
#include <optional>

#include <climits>

#include "elf.hpp"
#include "ostream.hpp"
#include "bean.hpp"

#include "strptr.hpp"
#include "bufstream.hpp"

#include "object_identity.hpp"
#include "versioned_symbol.hpp"
#include "memory_segment.hpp"


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

		/*! \brief Pointer to data */
		void * ptr = nullptr;

		/*! \brief File data hash */
		uint64_t hash = 0;
	} data;

	/*! \brief Begin offset of virtual memory area (for dynamic objects) */
	uintptr_t base = 0;

	/*! \brief File relative address of global offset table (for dynamic objects) */
	uintptr_t global_offset_table = 0;

	/*! \brief status flags */
	bool is_prepared = false;
	bool is_protected = false;

	/*! \brief Symbol dependencies to other objects */
	std::vector<ObjectIdentity *> dependencies;

	/*! \brief Segments to be loaded in memory */
	std::vector<MemorySegment> memory_map;

	/*! \brief Binary symbol hashes */
	std::optional<Bean> binary_hash;

	/*! \brief Relocations to external symbols used in this object (cache) */
	mutable std::vector<std::pair<Elf::Relocation, VersionedSymbol>> relocations;

	/*! \brief Pointer to previous version */
	Object * file_previous = nullptr;

	/*! \brief create new object */
	Object(ObjectIdentity & file, const Data & data);

	Object(const Object&) = delete;
	Object& operator=(const Object&) = delete;

	/*! \brief destroy object */
	virtual ~Object();

	/*! check if this object is the current (latest) version */
	bool is_latest_version() const {
		return this == file.current;
	}

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
	virtual void update() { };

	/*! \brief Set protection flags in memory */
	bool protect();

	/*! \brief Initialisation method */
	virtual bool initialize() { return true; };

	/*! \brief Check if current object can patch a previous version */
	virtual bool patchable() const { return false; };

	/*! \brief Find (external visible) symbol in this object with same name and version */
	virtual std::optional<VersionedSymbol> resolve_symbol(const VersionedSymbol & sym) const { return std::nullopt; };

	bool operator==(const Object & o) const {
		return this == &o;
	}

	bool operator!=(const Object & o) const {
		return !operator==(o);
	}
};

static inline BufferStream& operator<<(BufferStream& bs, const Object & o) {
	return bs << "[Object " << o.file << "]";
}
