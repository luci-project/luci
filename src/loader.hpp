#pragma once

#include <vector>
#include <list>
#include <unordered_map>
#include <map>
#include <optional>

#include <pthread.h>

#include "elf.hpp"

#include "dl.hpp"
#include "versioned_symbol.hpp"
#include "object_identity.hpp"

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

	/*! \brief Aquire lock for accessing / modifing the data structures */
	void lock() const {
		pthread_mutex_lock(&mutex);
	}

	/*! \brief Unlock */
	void unlock() const {
		pthread_mutex_unlock(&mutex);
	}

 private:
	friend void * observer_kickoff(void * ptr);
	friend struct ObjectIdentity;

	/*! \brief mutex*/
	mutable pthread_mutex_t mutex;

	/*! \brief Descriptor for inotify */
	int inotifyfd;

	/*! \brief thread pointer*/
	pthread_t observer_thread;

	/*! \brief observer method */
	void observer();

	/*! \brief get new namespace */
	DL::Lmid_t get_new_ns() const;
};
