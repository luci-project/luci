#pragma once

#include <dlh/container/optional.hpp>
#include <dlh/container/vector.hpp>
#include <dlh/container/tree.hpp>
#include <dlh/container/list.hpp>
#include <dlh/socket_client.hpp>
#include <dlh/rwlock.hpp>
#include <dlh/mutex.hpp>
#include <dlh/thread.hpp>

#include "object/identity.hpp"
#include "versioned_symbol.hpp"
#include "trampoline.hpp"
#include "tls.hpp"

struct Loader {
	const struct Config {
		/*! \brief enable dynamic updates? */
		bool dynamic_update = false;

		/*! \brief enable dynamic updates of functions using the dl* interface? */
		bool dynamic_dlupdate = false;

		/*! \brief check all dependencies when comparing of functions*/
		bool dependency_check = false;

		/*! \brief force dynamic updates even if they seem incompatible */
		bool force_update = false;

		/*! \brief support dynamic weak definitions? */
		bool dynamic_weak = false;

		/*! \brief check if content of relocation target in data section has been altered by the user */
		bool check_relocation_content = false;

		/*! \brief update relocations in outdated (old) versions as well? */
		bool update_outdated_relocations = false;

		/*! \brief detect execution of outdated files?*/
		bool detect_outdated_access = false;

		/* Default constructor */
		Config() {}
	} config;

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

	/*! \brief loader object */
	ObjectIdentity * self;

	/*! \brief object for main program */
	ObjectIdentity * target = nullptr;

	/*! \brief synchronize lookup access */
	mutable RWLock lookup_sync;

	/*! \brief thread local storage */
	TLS tls;

	/*! \brief Trampoline to dynamically loaded symbols (using dlsym) - for dynamic_dlupdate */
	Trampoline dlsyms;

	/*! \brief socket to receive elf hash */
	Socket::Client debug_hash_socket;

	/*! \brief Descriptor for status info output */
	int statusinfofd = -1;

	/*! \brief Descriptor for inotify */
	int filemodification_inotifyfd = -1;

	/*! \brief Descriptor for userfaultfd */
	int userfaultfd = -1;

	/*! \brief start arguments & environment pointer*/
	int argc = 0;
	const char ** argv = nullptr;
	const char ** envp = nullptr;

	/*! \brief Default flags for objects */
	ObjectIdentity::Flags default_flags;

	/*! \brief Constructor */
	Loader(uintptr_t self, const char * sopath = "/lib/ld-luci.so", Config config = Config{});

	/*! \brief Destructor: Unload all files */
	~Loader();

	/*! \brief Search & load libary */
	ObjectIdentity * library(const char * file, ObjectIdentity::Flags flags, const Vector<const char *> & rpath = {}, const Vector<const char *> & runpath = {}, namespace_t ns = NAMESPACE_BASE, bool load = true);
	inline ObjectIdentity * library(const char * file) {
		return library(file, default_flags);
	}

	/*! \brief Load file */
	ObjectIdentity * open(const char * filename, const char * directory, ObjectIdentity::Flags flags, namespace_t ns = NAMESPACE_BASE);
	ObjectIdentity * open(const char * path, ObjectIdentity::Flags flags, namespace_t ns = NAMESPACE_BASE, uintptr_t addr = 0, Elf::ehdr_type type = Elf::ET_NONE);
	inline ObjectIdentity * open(const char * path) {
		return open(path, default_flags);
	}
	//ObjectIdentity * open(uintptr_t addr, ObjectIdentity::Flags flags, const char * filepath = nullptr, namespace_t ns = NAMESPACE_BASE, Elf::ehdr_type type = Elf::ET_NONE);

	/*! \brief Search, load & initizalize libary (during runtime) */
	ObjectIdentity * dlopen(const char * file, ObjectIdentity::Flags flags, namespace_t ns = NAMESPACE_BASE, bool load = true);

	/*! \brief Run */
	bool run(ObjectIdentity * file, const Vector<const char *> & args, uintptr_t stack_pointer = 0, size_t stack_size = 0);
	bool run(ObjectIdentity * file, uintptr_t stack_pointer);

	/*! \brief find Symbol with same name and version from other objects in same namespace */
	enum ResolveSymbolMode {
		RESOLVE_DEFAULT,
		RESOLVE_AFTER_OBJECT,
		RESOLVE_EXCEPT_OBJECT,
		RESOLVE_OBJECT_FIRST,
		RESOLVE_NO_DEPENDENCIES,
	};
	inline Optional<VersionedSymbol> resolve_symbol(const VersionedSymbol & sym, namespace_t ns = NAMESPACE_BASE, const ObjectIdentity * obj = nullptr, ResolveSymbolMode mode = RESOLVE_DEFAULT) const {
		return resolve_symbol(sym.name(), sym.hash_value(), sym.gnu_hash_value(), sym.version, ns, obj, mode);
	}
	inline Optional<VersionedSymbol> resolve_symbol(const char * name, const char * version = nullptr, namespace_t ns = NAMESPACE_BASE, const ObjectIdentity * obj = nullptr, ResolveSymbolMode mode = RESOLVE_DEFAULT) const {
		return resolve_symbol(name, ELF_Def::hash(name), ELF_Def::gnuhash(name), VersionedSymbol::Version(version), ns, obj, mode);
	}
	Optional<VersionedSymbol> resolve_symbol(const char * name, uint32_t hash, uint32_t gnu_hash, const VersionedSymbol::Version & version, namespace_t ns = NAMESPACE_BASE, const ObjectIdentity * obj = nullptr, ResolveSymbolMode mode = RESOLVE_DEFAULT) const;

	/*! \brief find Symbol overlapping the given address in same namespace */
	Optional<VersionedSymbol> resolve_symbol(uintptr_t addr, namespace_t ns = NAMESPACE_BASE) const;

	/*! \brief find Object overlapping the given address in same namespace */
	Object * resolve_object(uintptr_t addr, namespace_t ns = NAMESPACE_BASE) const;

	/*! \brief get next (page aligned) memory address */
	uintptr_t next_address() const;

	/*! \brief check if object is already loaded */
	bool is_loaded(const ObjectIdentity * ptr) const;

	/*! \brief get instance for current process */
	static Loader * instance();

	/*! \brief Start handler threads */
	bool start_handler_threads();

 private:
	friend void* kickoff_filemodification_handler(void * ptr);
	friend void* kickoff_userfault_handler(void * ptr);

	/*! \brief Next Namespace */
	mutable namespace_t next_namespace;

	/*! \brief Main thread (for TLS) */
	Thread * main_thread = nullptr;

	/*! \brief file modification handler (executed in thread) */
	void filemodification_handler();

	/*! \brief userfault handler (executed in thread) */
	void userfault_handler();

	/*! \brief relocate all loaded files for execution */
	bool relocate(bool update = false);

	/*! \brief prepare all loaded files for execution */
	bool prepare();
};
