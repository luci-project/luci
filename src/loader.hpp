#pragma once

#include <dlh/container/optional.hpp>
#include <dlh/container/vector.hpp>
#include <dlh/container/tree.hpp>
#include <dlh/container/list.hpp>
#include <dlh/utils/mutex.hpp>
#include <dlh/utils/thread.hpp>

#include "object/identity.hpp"

#include "dl.hpp"
#include "versioned_symbol.hpp"

struct Loader {
	/*! \brief enable dynamic updates? */
	const bool dynamic_update;

	/*! \brief default library path via argument / environment variable */
	Vector<const char *> library_path_runtime;

	/*! \brief default library path from config */
	Vector<const char *> library_path_config;

	/*! \brief default library path default (by convention) */
	Vector<const char *> library_path_default = { "/lib" , "/usr/lib" };

	/*! \brief libraries to exclude */
	Vector<const char *> library_exclude = { "ld-linux-x86-64.so.2" , "libdl.so.2" };

	/*! \brief List of all loaded objects (for symbol resolving) */
	ObjectIdentityList lookup;

	/*! \brief mutex*/
	mutable Mutex mutex;

	/*! \brief Constructor */
	Loader(void * self, bool dynamicUpdate = false);

	/*! \brief Destructor: Unload all files */
	~Loader();

	/*! \brief Search & load libary */
	ObjectIdentity * library(const char * file, const Vector<const char *> & rpath = {}, const Vector<const char *> & runpath = {}, DL::Lmid_t ns = DL::LM_ID_BASE);

	/*! \brief Load file */
	ObjectIdentity * open(const char * filename, const char * directory, DL::Lmid_t ns = DL::LM_ID_BASE);
	ObjectIdentity * open(const char * path, DL::Lmid_t ns = DL::LM_ID_BASE);
	ObjectIdentity * open(void * ptr, bool prevent_updates, bool is_prepared, bool is_mapped, const char * filepath = nullptr, DL::Lmid_t ns = DL::LM_ID_BASE, Elf::ehdr_type type = Elf::ET_NONE);

	/*! \brief prepare all loaded files for execution */
	bool prepare();

	/*! \brief Run */
	bool run(ObjectIdentity * file, const Vector<const char *> & args, uintptr_t stack_pointer = 0, size_t stack_size = 0);
	bool run(ObjectIdentity * file, uintptr_t stack_pointer);

	/*! \brief find Symbol with same name and version from other objects in same namespace
	 */
	Optional<VersionedSymbol> resolve_symbol(const VersionedSymbol & sym, DL::Lmid_t ns = DL::LM_ID_BASE) const;

	/*! \brief get next (page aligned) memory address */
	uintptr_t next_address() const;

 private:
	friend int observer_kickoff(void * ptr);
	friend struct ObjectIdentity;

	/*! \brief Descriptor for inotify */
	int inotifyfd;

	/*! \brief observer method */
	void observer();

	/*! \brief get new namespace */
	DL::Lmid_t get_new_ns() const;
};
