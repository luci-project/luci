#pragma once

#include <string>
#include <vector>
#include <elfio/elfio.hpp>

#include "segment.hpp"


struct Object {
	/*! \brief default library path */
	static std::vector<std::string> librarypath;

	/*! \brief List of all loaded objects (for symbol resolving) */
	static std::vector<Object *> objects;

	/*! \brief Search & load libary */
	static bool load_library(std::string lib, const std::vector<std::string> & rpath = {}, const std::vector<std::string> & runpath = {});

	/*! \brief Load file */
	static bool load_file(std::string path);

	/*! \brief Unload all files */
	static void unload_all();

	static bool run_all(std::vector<std::string> args, uintptr_t stack_pointer = 0, size_t stack_size = 0);

	/*! \brief Full path to file */
	std::string path;

	/*! \brief Elf accessor */
	ELFIO::elfio elf;

	/*! \brief File deskriptor */
	int fd;

	/*! \brief Segments to be loaded in memory */
	std::vector<Segment> segments;

	/*! \brief get file name of object */
	std::string get_file_name();

	/*! \brief virtual memory range used by this object */
	bool get_memory_range(uintptr_t & start, uintptr_t & end);

	/*! \brief allocate in memory */
	bool allocate(bool copy = true);

	/*! \brief Relocate sections */
	bool relocate();

	/*! \brief Set protection flags in memory */
	bool protect();

	/*! \brief Run */
	bool run(std::vector<std::string> args, uintptr_t stack_pointer = 0, size_t stack_size = 0);

 protected:
	/*! \brief create new object */
	Object(std::string path, int fd);

	/*! \brief destroy object */
	virtual ~Object();

	/*! \brief Initialisation */
	virtual bool load() = 0;

	/*! \brief get next (page aligned) memory address */
	static uintptr_t get_next_address();
};
