#pragma once

#include <utility>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <optional>

#include "elf.hpp"

#include "dl.hpp"
#include "symbol.hpp"
#include "memory_segment.hpp"

struct Loader;

struct Object {
	const struct File {
		/*! \brief Full path to file */
		const char * path;

		/*! \brief File hash */
		uint64_t hash;

		/*! \brief File deskriptor */
		int fd;

		/*! \brief Pointer to data */
		const void * data;

		/*! \brief Namespace for object */
		DL::Lmid_t ns;

		/*! \brief Parent */
		//const Object * parent;

		/*! \brief Loader */
		const Loader * loader;

		/*! \brief extract file name */
		const char * name() const {
			const char * r = path;
			if (r != nullptr)
				for (const char * i = path; *i != '\0'; ++i)
					if (*i == '/')
						r = i + 1;
			return r;
		}

		/*! \brief constructor */
		File(const Loader * loader = nullptr, DL::Lmid_t ns = DL::LM_ID_NEWLN, const char * path = nullptr, uint64_t hash = 0, int fd = -1, void * data = nullptr) : path(path), hash(hash), fd(fd), data(data), ns(ns), loader(loader) {}
	} file;

	/*! \brief Elf accessor */
	const Elf elf;

	/*! \brief Begin offset of virtual memory area (for dynamic objects) */
	uintptr_t base = 0;

	/*! \brief File relative address of global offset table (for dynamic objects) */
	uintptr_t global_offset_table = 0;

	/*! \brief Library dependencies */
	std::vector<Object *> dependencies;

	/*! \brief Segments to be loaded in memory */
	std::vector<MemorySegment> memory_map;

	/*! \brief virtual memory range used by this object */
	bool memory_range(uintptr_t & start, uintptr_t & end) const;

	/*! \brief resolve dynamic relocation entry (if possible!) */
	virtual void* dynamic_resolve(size_t index) const;

 protected:
	friend struct Loader;

	/*! \brief create new object */
	Object(const File & file = File()) : file(file), elf(reinterpret_cast<uintptr_t>(file.data)) {
		assert(file.data != nullptr);
	}

	Object(const Object&) = delete;
	Object& operator=(const Object&) = delete;


	/*! \brief destroy object */
	virtual ~Object();

	/*! \brief Initialisation of object (after creation) */
	virtual bool preload() = 0;

	/*! \brief allocate required segments in memory */
	bool run_allocate();

	/*! \brief Relocate  */
	virtual bool run_relocate(bool bind_now = false) { return true; };

	/*! \brief Set protection flags in memory */
	bool run_protect();

	/*! \brief Initialisation (before execution) */
	virtual bool run_init() { return true; };

	/*! \brief Find (external visible) symbol in this object with same name and version */
	virtual std::optional<Symbol> resolve_symbol(const Symbol & sym) const { return std::nullopt; };
};
