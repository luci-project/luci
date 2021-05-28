#pragma once

#include <vector>
#include <list>
#include <map>
#include <optional>

#include <pthread.h>

#include "utils/mutex.hpp"

#include "object/identity.hpp"

#include "dl.hpp"
#include "versioned_symbol.hpp"

struct Loader {
	/*! \brief enable dynamic updates? */
	const bool dynamic_update;

	/*! \brief default library path via argument / environment variable */
	std::vector<const char *> library_path_runtime;

	/*! \brief default library path from config */
	std::vector<const char *> library_path_config;

	/*! \brief default library path default (by convention) */
	std::vector<const char *> library_path_default = { "/lib" , "/usr/lib" };

	/*! \brief List of all loaded objects (for symbol resolving) */
	std::list<ObjectIdentity> lookup;

	/*! \brief mutex*/
	mutable Mutex mutex;

	/*! \brief Constructor */
	Loader(const char * path, bool dynamicUpdate = false);

	/*! \brief Destructor: Unload all files */
	~Loader();

	/*! \brief Search & load libary */
	ObjectIdentity * library(const char * file, const std::vector<const char *> & rpath = {}, const std::vector<const char *> & runpath = {}, DL::Lmid_t ns = DL::LM_ID_BASE);

	/*! \brief Load file */
	ObjectIdentity * open(const char * filename, const char * directory, DL::Lmid_t ns = DL::LM_ID_BASE);
	ObjectIdentity * open(const char * path, DL::Lmid_t ns = DL::LM_ID_BASE);
	ObjectIdentity * open(void * ptr, bool prevent_updates, bool in_execution, const char * filepath = nullptr, DL::Lmid_t ns = DL::LM_ID_BASE);

	/*! \brief prepare all loaded files for execution */
	bool prepare();

	/*! \brief Run */
	bool run(ObjectIdentity * file, std::vector<const char *> args, uintptr_t stack_pointer = 0, size_t stack_size = 0);

	/*! \brief find Symbol with same name and version from other objects in same namespace
	 */
	std::optional<VersionedSymbol> resolve_symbol(const VersionedSymbol & sym, DL::Lmid_t ns = DL::LM_ID_BASE) const;

	/*! \brief get next (page aligned) memory address */
	uintptr_t next_address() const;

 private:
	friend void * observer_kickoff(void * ptr);
	friend struct ObjectIdentity;

	/*! \brief Descriptor for inotify */
	int inotifyfd;

	/*! \brief thread pointer*/
	pthread_t observer_thread;

	/*! \brief observer method */
	void observer();

	/*! \brief get new namespace */
	DL::Lmid_t get_new_ns() const;
};
