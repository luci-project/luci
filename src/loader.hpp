// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <dlh/container/optional.hpp>
#include <dlh/container/vector.hpp>
#include <dlh/container/hash.hpp>
#include <dlh/container/list.hpp>
#include <dlh/container/pair.hpp>
#include <dlh/container/tree.hpp>
#include <dlh/socket_client.hpp>
#include <dlh/mutex_rec.hpp>
#include <dlh/rwlock.hpp>
#include <dlh/thread.hpp>

#include "object/identity.hpp"
#include "trampoline.hpp"
#include "redirect.hpp"
#include "symbol.hpp"
#include "tls.hpp"

extern "C" int __luci_update();

struct Loader {
	const struct Config {
		/*! \brief use position independent code? */
#if defined(COMPATIBILITY_DEBIAN) || defined(COMPATIBILITY_UBUNTU)
		bool position_independent = true;
#else
		bool position_independent = false;
#endif

		/*! \brief enable dynamic updates? */
		bool dynamic_update = false;

		/*! \brief enable dynamic updates of functions using the dl* interface? */
		bool dynamic_dlupdate = false;

		/*! \brief check all dependencies when comparing of functions*/
		bool dependency_check = false;

		/*! \brief use vDSO */
		bool enable_vdso = true;

		/*! \brief force dynamic updates even if they seem incompatible */
		bool force_update = false;

		/*! \brief skip updates identical to previously loaded versions? */
		bool skip_identical = false;

		/*! \brief support dynamic weak definitions? */
		bool dynamic_weak = false;

		/*! \brief Stop process during update (required for cross processor code modification according to intel)? */
		bool stop_on_update = false;

		/*! \brief use modification time to detect changes? */
		bool use_mtime = false;

		/*! \brief check if content of relocation target in data section has been altered by the user */
		bool check_relocation_content = false;

		/*! \brief update relocations in outdated (old) versions as well? */
		bool update_outdated_relocations = false;

		/*! \brief mode for updates */
		enum UpdateMode {
			UPDATE_MODE_GOT,                // just update global offset table
			UPDATE_MODE_CODEREL,            // update relocations in machine code
			UPDATE_MODE_CODEREL_LOCALINT,   // update relocations in machine code and intercept local branches
		} update_mode = UPDATE_MODE_GOT;

		/*! \brief detect execution of outdated files? (value is delay in seconds after update) */
		enum DetectOutdated {
			DETECT_OUTDATED_DISABLED,
			DETECT_OUTDATED_VIA_USERFAULTFD,
			DETECT_OUTDATED_VIA_UPROBES,
			DETECT_OUTDATED_WITH_DEPS_VIA_UPROBES,
			DETECT_OUTDATED_VIA_PTRACE
		} detect_outdated = DETECT_OUTDATED_DISABLED;

		/*! \brief delay (in seconds) after an update before enabling detection of access of outdated libs */
		unsigned detect_outdated_delay = 1;

		/*! \brief Trap for code redirection */
		enum Redirect::Mode trap_mode = Redirect::MODE_BREAKPOINT_TRAP;

		/*! \brief set comparison mode to relax patchability checks */
		int relax_comparison = 0;

		/*! \brief output status info during initialization */
		bool early_statusinfo = false;

		/*! \brief look for external debug symbols (for bean hashing)? */
		bool find_debug_symbols = false;

		/*! \brief Show arguments before starting process? */
		bool show_args = false;

		/*! \brief Show environment variables before starting process? */
		bool show_env = false;

		/*! \brief Show auxiliary vectors before starting process? */
		bool show_auxv = false;

		/*! \brief Support debuger by preserving older versions on the file system? */
		bool debugger = true;

		/*! \brief Root directory for debug symbols (if nullptr system root is used) */
		const char * debug_symbols_root = nullptr;

		/* Default constructor */
		Config() {}
	} config;

	/*! \brief default library path via argument / environment variable */
	Vector<const char *> library_path_runtime;

	/*! \brief default library path from config */
	Vector<const char *> library_path_config;

	/*! \brief default library path default (by convention) */
	Vector<const char *> library_path_default = {
		"/lib", "/usr/lib",
	#ifdef PLATFORM_X64
		"/lib64", "/usr/lib64",
	#endif
	};

	/*! \brief libraries to exclude in dependencies */
	HashSet<const char *> library_exclude{ "ld-linux-x86-64.so.2" , "libdl.so.2" };

	/*! \brief List of all loaded objects (for symbol resolving) */
	ObjectIdentityList lookup;

	/*! \brief loader object */
	ObjectIdentity * self;

	/*! \brief object for main program */
	ObjectIdentity * target = nullptr;

	/*! \brief synchronize lookup access */
	mutable RWLock<MutexRecursive> lookup_sync;

	/*! \brief thread local storage */
	TLS tls;

	/*! \brief Trampoline used for dynamically loaded symbols (using dlsym) - for dynamic_dlupdate */
	Trampoline symbol_trampoline;

	/*! \brief socket to receive elf hash */
	Socket::Client debug_hash_socket;

	/*! \brief Main thread (for TLS) */
	Thread * main_thread = nullptr;

	/*! \brief Pointer to handler thread */
	Thread * handler_thread = nullptr;

	/*! \brief Process PID */
	pid_t pid;

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

	/*! \brief has the process control? */
	bool process_started = false;

	/*! \brief are updates currently pending? */
	bool update_pending = false;

	/*! \brief Default flags for objects */
	ObjectIdentity::Flags default_flags;

	/*! \brief Constructor */
	explicit Loader(uintptr_t self, const char * sopath = "/lib/ld-luci.so", Config config = Config{});

	/*! \brief Destructor: Unload all files */
	~Loader();

	/*! \brief Search & load libary */
	ObjectIdentity * library(const char * file, ObjectIdentity::Flags flags, bool priority = false, const Vector<const char *> & rpath = {}, const Vector<const char *> & runpath = {}, namespace_t ns = NAMESPACE_BASE, bool load = true);
	inline ObjectIdentity * library(const char * file) {
		return library(file, default_flags);
	}

	/*! \brief Load file */
	ObjectIdentity * open(const char * filename, const char * directory, ObjectIdentity::Flags flags, bool priority = false, namespace_t ns = NAMESPACE_BASE, const char * altname = nullptr);
	ObjectIdentity * open(const char * path, ObjectIdentity::Flags flags, bool priority = false, namespace_t ns = NAMESPACE_BASE, uintptr_t addr = 0, Elf::ehdr_type type = Elf::ET_NONE, const char * altname = nullptr);
	inline ObjectIdentity * open(const char * path) {
		return open(path, default_flags);
	}
	// ObjectIdentity * open(uintptr_t addr, ObjectIdentity::Flags flags, const char * filepath = nullptr, namespace_t ns = NAMESPACE_BASE, Elf::ehdr_type type = Elf::ET_NONE);

	/*! \brief Search, load & initizalize libary (during runtime) */
	ObjectIdentity * dlopen(const char * file, ObjectIdentity::Flags flags, namespace_t ns = NAMESPACE_BASE, bool load = true);

	/*! \brief Run */
	bool run(ObjectIdentity * file, const Vector<const char *> & args, uintptr_t stack_pointer = 0, size_t stack_size = 0, const char * entry_point = nullptr);
	bool run(ObjectIdentity * file, uintptr_t stack_pointer, const char * entry_point = nullptr);

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
	uintptr_t next_address(size_t size = 0) const;
	/*! \brief reset start address (required to free allocated address on aborting loading) */
	void reset_address(uintptr_t addr) const;

	/*! \brief check if object is already loaded */
	bool is_loaded(const ObjectIdentity * ptr) const;

	/*! \brief get instance for current process */
	static Loader * instance();

	/*! \brief Start handler threads */
	bool start_handler_threads();

 private:
	friend void* kickoff_helper_loop(void * ptr);
	friend int __luci_update();

	/*! \brief Iterator to first pure dependency library in lookup list */
	ObjectIdentityList::Iterator dependencies;

	/*! \brief Next Namespace */
	mutable namespace_t next_namespace;

	/*! \brief next unused address for object */
	mutable uintptr_t next_library_address = 0;

	/*! \brief helper loop for file modification detection and userfault handling (executed in new thread) */
	void helper_loop();

	/*! \brief File modification detection (called in helper loop) */
	void filemodification_detect(unsigned long now, TreeSet<Pair<unsigned long, ObjectIdentity*>> & worklist_load);

	/*! \brief Delayed object loading after file modifiaction (called in helper loop) */
	void filemodification_load(unsigned long now, TreeSet<Pair<unsigned long, ObjectIdentity*>> & worklist_load, TreeSet<Pair<unsigned long, Object*>> & worklist_protect);
	bool filemodification_load_helper(ObjectIdentity* object, uintptr_t addr = 0);

	/*! \brief Delayed object protection after file modifiaction (called in helper loop) */
	void filemodification_protect(unsigned long now, TreeSet<Pair<unsigned long, Object*>> & worklist_protect);

	/*! \brief userfault handler (called in helper loop) */
	void userfault_handle();

	/*! \brief relocate all loaded files for execution */
	bool relocate(bool update = false);

	/*! \brief Perform update */
	void update();

	/*! \brief resolve address of entry point */
	uintptr_t get_entry_point(Object * start, const char * custom_entry_point = nullptr);

	/*! \brief prepare all loaded files for execution */
	bool prepare(Object * start);

	/*! \brief dump process contents of initial stack */
	void show_init_stack(uintptr_t entry, uintptr_t stack_pointer, const char ** envp) const;
};
