#pragma once


#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <map>
#include <optional>

#include <pthread.h>

#include "elf.hpp"

#include "dl.hpp"
#include "versioned_symbol.hpp"
#include "object_file.hpp"

struct Loader {
	/*! \brief enable dynamic updates? */
	const bool dynamic_update;

	/*! \brief default library path via argument / environment variable */
	std::vector<const char *> library_path_runtime = {};

	/*! \brief default library path from config */
	std::vector<const char *> library_path_config = {};

	/*! \brief default library path default (by convention) */
	std::vector<const char *> library_path_default  = { "/lib", "/usr/lib" };

	/*! \brief List of all loaded objects (for symbol resolving) */
	std::list<ObjectFile> lookup;

	/*! \brief Constructor */
	Loader(const char * self, bool dynamicUpdate = false);

	/*! \brief Destructor: Unload all files */
	~Loader();

	/*! \brief Search & load libary */
	ObjectFile * library(const char * file, const std::vector<const char *> & rpath, const std::vector<const char *> & runpath, DL::Lmid_t ns = DL::LM_ID_BASE);
	ObjectFile * library(const char * file, const std::vector<const char *> & search = {}, DL::Lmid_t ns = DL::LM_ID_BASE);

	/*! \brief Load file */
	ObjectFile * file(const char * filename, const char * directory, DL::Lmid_t ns = DL::LM_ID_BASE);
	ObjectFile * file(const char * path, DL::Lmid_t ns = DL::LM_ID_BASE);

	/*! \brief prepare all loaded files for execution */
	bool prepare();

	/*! \brief Run */
	bool run(const ObjectFile * object_file, std::vector<const char *> args, uintptr_t stack_pointer = 0, size_t stack_size = 0);

	/*! \brief find Symbol with same name and version from other objects in same namespace
	 */
	std::optional<VersionedSymbol> resolve_symbol(const VersionedSymbol & sym, DL::Lmid_t ns = DL::LM_ID_BASE) const;

	/*! \brief get next (page aligned) memory address */
	uintptr_t next_address() const;

	/*! \brief Aquire lock for accessing / modifing the data structures */
	void lock() const {
		pthread_mutex_lock(&mutex);
	}

	/*! \brief Unlock */
	void unlock() const {
		pthread_mutex_unlock(&mutex);
	}

 private:
	friend struct ObjectFile;
	friend void * observer_kickoff(void * ptr);

	int inotifyfd;
	pthread_t observer_thread;
	mutable pthread_mutex_t mutex;

	void observer();
};
