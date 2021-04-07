#pragma once

#include <utility>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>

#include "elf.hpp"

#include "dl.hpp"
#include "symbol.hpp"
#include "memory_segment.hpp"


struct Object {
	/*! \brief default library path via argument / environment variable */
	static std::vector<std::string> library_path_runtime;

	/*! \brief default library path from config */
	static std::vector<std::string> library_path_config;

	/*! \brief default library path default (by convention) */
	static std::vector<std::string> library_path_default;

	/*! \brief List of all loaded objects (for symbol resolving) */
	static std::vector<Object *> objects;

	static std::vector<std::vector<Object *>> lookup;

	static void library_path(const std::string & runtime = "", const std::string & config_file = "ld.so.conf");

	/*! \brief Search & load libary */
	static Object * load_library(const std::string & file, const std::vector<std::string> & rpath, const std::vector<std::string> & runpath, DL::Lmid_t ns = DL::LM_ID_BASE);
	static Object * load_library(const std::string & file, const std::vector<std::string> & search = {}, DL::Lmid_t ns = DL::LM_ID_BASE);

	/*! \brief Load file */
	static Object * load_file(const std::string & path, DL::Lmid_t ns = DL::LM_ID_BASE);

	/*! \brief find Symbol */
	static std::pair<Object*,Symbol> find_symbol(const std::string & name, bool only_defined = true);

	/*! \brief Unload all files */
	static void unload_all();

	/*! \brief Check if valid (loaded) object */
	static bool valid(const Object * o);
	static bool valid(const Object & o);

	/*! \brief Full path to file */
	const std::string path;

	/*! \brief File deskriptor */
	const int fd;

	/*! \brief Elf accessor */
	const Elf elf;

	/*! \brief Namespace for object */
	const DL::Lmid_t ns;

	/*! \brief Begin offset of virtual memory area (for dynamic objects) */
	uintptr_t base = 0;

	/*! \brief File relative address of global offset table (for dynamic objects) */
	uintptr_t global_offset_table = 0;

	/*! \brief Library dependencies */
	std::vector<Object *> dependencies;

	/*! \brief Segments to be loaded in memory */
	std::vector<MemorySegment> memory_map;

	/*! \brief get file name of object */
	std::string file_name() const;

	/*! \brief virtual memory range used by this object */
	bool memory_range(uintptr_t & start, uintptr_t & end) const;

	/*! \brief Run */
	bool run(std::vector<std::string> args, uintptr_t stack_pointer = 0, size_t stack_size = 0);

	/*! \brief resolve dynamic relocation entry (if possible!) */
	virtual void* resolve(size_t index) const;

 protected:
	Object() : fd(-1), elf(0), ns(DL::LM_ID_NEWLN) {
		assert(false);
	};

	Object(const Object&) = delete;
	Object& operator=(const Object&) = delete;

	/*! \brief create new object */
	Object(std::string path, int fd, void * mem, DL::Lmid_t ns);

	/*! \brief destroy object */
	virtual ~Object();

	/*! \brief Initialisation */
	virtual bool preload() = 0;

	/*! \brief allocate in memory */
	bool allocate();

	/*! \brief Relocation */
	virtual bool relocate() { return true; };

	/*! \brief Set protection flags in memory */
	bool protect();

	/*! \brief Initialisation */
	virtual bool init() { return true; };

	/*! \brief get symbol from this object with same name and version
	 */
	virtual Symbol symbol(const Symbol & sym) const { return Symbol(*this); };

	/*! \brief find Symbol with same name and version from other objects in same namespace
	 */
	Symbol find_symbol(const Symbol & sym) const;

	/*! \brief get next (page aligned) memory address */
	static uintptr_t next_address();
};
