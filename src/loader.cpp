// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "loader.hpp"

#include <dlh/stream/output.hpp>
#include <dlh/stream/buffer.hpp>
#include <dlh/parser/string.hpp>
#include <dlh/parser/ar.hpp>
#include <dlh/auxiliary.hpp>
#include <dlh/syscall.hpp>
#include <dlh/xxhash.hpp>
#include <dlh/error.hpp>
#include <dlh/file.hpp>
#include <dlh/math.hpp>
#include <dlh/page.hpp>
#include <dlh/log.hpp>

#include "comp/glibc/rtld/global.hpp"
#include "comp/glibc/rtld/dl.hpp"
#include "comp/glibc/start.hpp"
#include "comp/glibc/init.hpp"
#include "comp/gdb.hpp"
#include "object/base.hpp"
#include "process.hpp"
#include "redirect.hpp"


static Loader * _instance = nullptr;
#ifndef NO_FPU
bool _cpu_supports_xsave = false;
#endif

void* kickoff_helper_loop(void * ptr) {
	reinterpret_cast<Loader *>(ptr)->helper_loop();
	return nullptr;
}

static uintptr_t symbol_trampoline_address_callback(size_t size) {
	return _instance->next_address(size);
}

Loader::Loader(uintptr_t luci_self, const char * sopath, struct Config config)
 : config(config), symbol_trampoline(symbol_trampoline_address_callback), dependencies(lookup.end()) {
	default_flags.bind_global = 1;

	if (config.dynamic_update) {
		default_flags.updatable = 1;
	} else {
		default_flags.immutable_source = 1;
	}
	start_handler_threads();

	// Flags for vDSO and Luci
	ObjectIdentity::Flags static_flags;
	static_flags.bind_global = 1;
	static_flags.persistent = 1;
	static_flags.immutable_source = 1;
	static_flags.initialized = 1;
	static_flags.premapped = 1;

	// Add vDSO (if available)
	if (config.enable_vdso) {
		if (auto vdso = Auxiliary::vector(Auxiliary::AT_SYSINFO_EHDR)) {
			if (open("linux-vdso.so.1", static_flags, true, NAMESPACE_BASE, vdso.value()) == nullptr)
				LOG_WARNING << "Unable to load vDSO!" << endl;
		}
	}

	// add self as dynamic exec
	assert(luci_self != 0);
	if ((self = open(sopath, static_flags, true, NAMESPACE_BASE, luci_self)) == nullptr) {
		LOG_ERROR << "Unable to load Luci (" << sopath << ")..." << endl;
		assert(false);
	}
	LOG_INFO << "Base of " << sopath << " is " << reinterpret_cast<void*>(self->current->base) << endl;
	self->current->base = 0;

	// Assign current instance
	_instance = this;

#ifndef NO_FPU
	// Check availability of xsave
	unsigned ecx = 0;
	asm volatile ("cpuid" : "=c"(ecx) : "a" (1) :  "%ebx", "%edx");
	_cpu_supports_xsave = (ecx & 0x04000000) != 0;
	LOG_DEBUG << "Preserving FPU using " << (_cpu_supports_xsave ? "XSAVE" : "FXSAVE") << endl;
#endif

	// Configure redirection
	if (config.dynamic_update)
		Redirect::setup(config.trap_mode);
}


Loader::~Loader() {
	_instance = nullptr;

	debug_hash_socket.disconnect();

	if (statusinfofd >= 0) {
		if (auto close = Syscall::close(statusinfofd)) {
			LOG_INFO << "Destroyed status info handle" << endl;
		} else {
			LOG_ERROR << "Closing status info handle failed: " << close.error_message() << endl;
		}
	}

	if (config.dynamic_update && filemodification_inotifyfd >= 0) {
		if (auto close = Syscall::close(filemodification_inotifyfd)) {
			LOG_INFO << "Destroyed file modification handle" << endl;
		} else {
			LOG_ERROR << "Closing file modification handle failed: " << close.error_message() << endl;
		}
	}

	if (config.detect_outdated == Loader::Config::DETECT_OUTDATED_VIA_USERFAULTFD && userfaultfd >= 0) {
		if (auto close = Syscall::close(userfaultfd)) {
			LOG_INFO << "Destroyed userfault handle" << endl;
		} else {
			LOG_ERROR << "Closing userfault handle failed: " << close.error_message() << endl;
		}
	}

	lookup.clear();
}


ObjectIdentity * Loader::library(const char * filename, ObjectIdentity::Flags flags, bool priority, const Vector<const char *> & rpath, const Vector<const char *> & runpath, namespace_t ns, bool load) {
	assert(filename != nullptr);
	StrPtr path(filename);
	auto name = path.find_last('/');
	if (ns != NAMESPACE_NEW)
		for (auto & i : lookup)
			if (ns == i.ns && name == i.name)
				return &i;

	if (load) {
		ObjectIdentity * lib;
		// only file name provided - look for it in search paths
		if (path == name) {
			for (const auto & path : { rpath, library_path_runtime, runpath, library_path_config, library_path_default }) {
				for (const auto & dir : path)
					if ((lib = open(filename, dir, flags, priority, ns)) != nullptr)
						return lib;
			}
		} else {
			// Full path
			if ((lib = open(filename, flags, priority, ns)) != 0)
				return lib;
		}

		LOG_ERROR << "Library '" << filename << "' cannot be found" << endl;
	}
	return nullptr;
}


ObjectIdentity * Loader::open(const char * filename, const char * directory, ObjectIdentity::Flags flags, bool priority, namespace_t ns) {
	auto filename_len = String::len(filename);
	auto directory_len = String::len(directory);

	char path[directory_len + filename_len + 2];  // NOLINT
	String::copy(path, directory, directory_len);
	path[directory_len] = '/';
	String::copy(path + directory_len + 1, filename, filename_len + 1);

	return open(path, flags, priority, ns);
}


ObjectIdentity * Loader::open(const char * filepath, ObjectIdentity::Flags flags, bool priority, namespace_t ns, uintptr_t addr, Elf::ehdr_type type) {  // NOLINT
	// Does file contain a valid full path or do we have a memory address?
	if (addr != 0 || File::exists(filepath)) {
		if (ns == NAMESPACE_NEW)
			ns = next_namespace++;

		LOG_DEBUG << "Loading " << filepath;
		if (addr != 0)
			LOG_DEBUG_APPEND << " from memory at " << reinterpret_cast<void*>(addr);
		LOG_DEBUG_APPEND << "..." << endl;

		// Check format
		auto format = type != Elf::ET_NONE ? File::contents::FORMAT_ELF : (addr != 0 ? File::contents::format(reinterpret_cast<const char *>(addr), 6) : File::contents::format(filepath));

		if (format == File::contents::FORMAT_AR) {
			if (addr != 0) {
				LOG_WARNING << "Loading static libraries mapped into memory not supported!" << endl;
			}
			AR archive(filepath);
			if (archive.is_valid()) {
				ObjectIdentity * obj = nullptr;
				for (auto & entry : archive) {
					if (entry.is_regular()) {
						LOG_DEBUG << "Loading " << entry.name() << " from library " << filepath << endl;
						obj = open(filepath, flags, priority, ns, reinterpret_cast<uintptr_t>(entry.data()));
					}
				}
				return obj;
			}
		} else if (format == File::contents::FORMAT_ELF) {
			GDB::notify(GDB::RT_ADD);
			auto i = lookup.emplace(priority ? dependencies : lookup.end(), *this, flags, filepath, ns, nullptr);
			assert(i);

			if (!priority && dependencies == lookup.end())
				dependencies = i;
			Object * o = i->load(addr, type);
			GDB::notify(GDB::RT_CONSISTENT);
			if (o != nullptr) {
				return i.operator->();
			} else {
				LOG_ERROR << "Unable to open " << filepath << endl;
				lookup.pop_back();
			}
		} else {
			LOG_ERROR << filepath << " has unsupported format: " << File::contents::format_description(format) << endl;
		}
	}
	return nullptr;
}


ObjectIdentity * Loader::dlopen(const char * file, ObjectIdentity::Flags flags, namespace_t ns, bool load) {
	ObjectIdentity * o = file == nullptr ? target : library(file, flags, false, {}, {}, ns, load);
	if (o != nullptr) {
		// Update flags
		o->flags.bind_now = flags.bind_now;  // TODO: if change to true, resolve all relocations
		o->flags.bind_global = flags.bind_global;
		o->flags.bind_deep = flags.bind_deep;
		o->flags.persistent = flags.persistent;
		o->flags.updatable = config.dynamic_dlupdate;

		if (load && !o->prepare()) {
			LOG_WARNING << "Preparation of " << o << " failed!" << endl;
			o = nullptr;
		}
	}

	if (load && o != nullptr && !o->initialize()) {
		LOG_WARNING << "Initialization of " << o << " failed!" << endl;
		o = nullptr;
	}

	return o;
}


bool Loader::relocate(bool update) {
	// Prepare
	for (auto & o : reverse(lookup))
		if (!o.prepare())
			return false;

	// Optional: Update relocations
	if (update) {
		for (auto & o : reverse(lookup)) {
			if (!o.update())
				return false;
		}

		// update dlsym trampolines
		symbol_trampoline.update();
	}

	// Protect memory
	for (auto & o : reverse(lookup))
		if (!o.finalize())
			return false;

	return true;
}

extern "C" uintptr_t __stack_chk_guard;
bool Loader::prepare(Object * start) {
	// Setup DLH vdso
	if (auto sym = resolve_symbol("__vdso_clock_gettime")) {
		Syscall::__clock_gettime = reinterpret_cast<int (*)(clockid_t, struct timespec *)>(sym->object().base + sym->value());
		LOG_DEBUG << "Using VDSO clock_gettime at " << reinterpret_cast<void*>(Syscall::__clock_gettime) << endl;
	}
	if (auto sym = resolve_symbol("__vdso_clock_getres")) {
		Syscall::__clock_getres = reinterpret_cast<int (*)(clockid_t, struct timespec *)>(sym->object().base + sym->value());
		LOG_DEBUG << "Using VDSO clock_getres at " << reinterpret_cast<void*>(Syscall::__clock_getres) << endl;
	}
	if (auto sym = resolve_symbol("__vdso_getcpu")) {
		Syscall::__getcpu = reinterpret_cast<int (*)(unsigned *, unsigned *, struct getcpu_cache *)>(sym->object().base + sym->value());
		LOG_DEBUG << "Using VDSO getcpu at " << reinterpret_cast<void*>(Syscall::__getcpu) << endl;
	}
	if (auto sym = resolve_symbol("__vdso_time")) {
		Syscall::__time = reinterpret_cast<time_t (*)(time_t *)>(sym->object().base + sym->value());
		LOG_DEBUG << "Using VDSO time at " << reinterpret_cast<void*>(Syscall::__time) << endl;
	}

	// Init GLIBC globals
	GLIBC::RTLD::init_globals(*this);

	// Initialize TLS
	main_thread = tls.allocate(nullptr, true);

	// Relocate all libraries
	if (!relocate(false))
		return false;

	auto random = Auxiliary::vector(Auxiliary::AT_RANDOM);
	if (random.valid()) {
		main_thread->setup_guards(random.a_un.a_ptr);
		// TODO: __stack_chk_guard = main_thread->stack_guard;
		LOG_DEBUG << "Stack Guard: " << reinterpret_cast<void*>(main_thread->stack_guard) << endl;
		LOG_DEBUG << "Pointer Guard: " << reinterpret_cast<void*>(main_thread->pointer_guard) << endl;
	} else {
		LOG_WARNING << "No AT_RANDOM!" << endl;
	}
	tls.dtv_setup(main_thread);
	GLIBC::RTLD::init_globals_tls(tls, main_thread->dtv);
	GLIBC::init(*this);

	// Initialize GDB
	GDB::init(*this);

	// Preinitizalize executable
	start->initialize(true);

	// Initialize (but not executable itself)
	for (auto & o : reverse(lookup))
		if (!o.initialize())
			return false;

	start->finalize();

	// Mark start of program
	GLIBC::RTLD::starting();

	return true;
}


uintptr_t Loader::get_entry_point(Object * start, const char * custom_entry_point) {
	uintptr_t entry = 0;
	if (custom_entry_point != nullptr) {
		// Check if symbol
		if (auto sym = resolve_symbol(custom_entry_point, nullptr, NAMESPACE_BASE, self, RESOLVE_EXCEPT_OBJECT)) {
			entry = reinterpret_cast<uintptr_t>(sym->pointer());
			LOG_VERBOSE << "Overwritten entry point with symbol " << custom_entry_point << sym->object() << endl;
		} else if (Parser::string(entry, custom_entry_point)) {
			LOG_VERBOSE << "Overwritten entry point with address " << reinterpret_cast<void*>(entry) << endl;
			// Check if relative (starting with '+' after trimming)
			for (const char * e = custom_entry_point; *e != '\0'; e++) {
				if (*e == '+')
					entry += start->base;
				if (*e != ' ')
					break;
			}
		} else {
			LOG_ERROR << "Unable to resolve custom entry " << custom_entry_point << " -- using default!" << endl;
		}
	}
	if (entry == 0) {
		if (start->header.type() == Elf::ET_REL) {
			// for relocatable objects imitate glibc _start
			entry = reinterpret_cast<uintptr_t>(GLIBC::start_entry(*this));
		} else {
			// for executable objects use provided entry function descriped in ELF header
			entry = start->base + start->header.entry();
		}
	}
	return entry;
}

template<typename T>
static void show_args(T &stream, uintptr_t stack_pointer) {
	size_t argc = *reinterpret_cast<size_t*>(stack_pointer);
	const char **argv = reinterpret_cast<const char**>(stack_pointer + sizeof(void*));

	for (size_t i = 0 ; i < argc; i++)
		stream << '\t' << (i + 1) << ". \"" << argv[i] << '"' << endl;
	stream << endl;
}

template<typename T>
static void show_env(T &stream, const char ** envp) {
	size_t envc = 0;
	for (; envp[envc] != nullptr; envc++)
		stream << '\t' << (envc + 1) << ". \"" << envp[envc] << "\" (0x" << reinterpret_cast<const void*>(envp[envc]) << ")" << endl;
	if (envc == 0)
		stream << "\t(none)" << endl;
	stream << endl;
}

template<typename T>
static void show_auxv(T &stream, const char ** envp) {
	size_t envc = 0;
	while (envp[envc] != nullptr)
		envc++;

	Auxiliary * auxv = reinterpret_cast<Auxiliary *>(envp + envc + 1);
	size_t auxc = 0;
	do {
		stream << '\t' << (auxc + 1) << ". ";
		auto v = auxv[auxc].a_un;
		switch (auxv[auxc].a_type) {
			case Auxiliary::AT_NULL:              stream << "NULL (End of vector)" << endl; break;
			case Auxiliary::AT_IGNORE:            stream << "IGNORE (Entry should be ignored)" << endl; break;
			case Auxiliary::AT_EXECFD:            stream << "EXECFD (File descriptor of program): " << v.a_val << endl; break;
			case Auxiliary::AT_PHDR:              stream << "PHDR (Program headers for program): 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_PHENT:             stream << "PHENT (Size of program header entry): " << v.a_val << endl; break;
			case Auxiliary::AT_PHNUM:             stream << "PHNUM (Number of program headers): " << v.a_val << endl; break;
			case Auxiliary::AT_PAGESZ:            stream << "PAGESZ (System page size): " << v.a_val << endl; break;
			case Auxiliary::AT_BASE:              stream << "BASE (Base address of interpreter): 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_FLAGS:             stream << "FLAGS: 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_ENTRY:             stream << "ENTRY (Entry point of program): 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_NOTELF:            stream << "NOTELF (Program is not ELF): 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_UID:               stream << "UID (Real UID): " << v.a_val << endl; break;
			case Auxiliary::AT_EUID:              stream << "EUID (Effective UID): " << v.a_val << endl; break;
			case Auxiliary::AT_GID:               stream << "GID (Real GID): " << v.a_val << endl; break;
			case Auxiliary::AT_EGID:              stream << "EGID (Effective GID): " << v.a_val << endl; break;
			case Auxiliary::AT_PLATFORM:          stream << "PLATFORM (String identifying platform): " << reinterpret_cast<const char *>(v.a_ptr) << endl; break;
			case Auxiliary::AT_HWCAP:             stream << "HWCAP (Machine-dependent hints about processor capabilities): 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_CLKTCK:            stream << "CLKTCK (Frequency of times): " << v.a_val << endl; break;
			case Auxiliary::AT_FPUCW:             stream << "FPUCW (Used FPU control word): 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_DCACHEBSIZE:       stream << "DCACHEBSIZE (Data cache block size): 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_ICACHEBSIZE:       stream << "ICACHEBSIZE (Instruction cache block size): 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_UCACHEBSIZE:       stream << "UCACHEBSIZE (Unified cache block size): 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_IGNOREPPC:         stream << "IGNOREPPC (Entry should be ignored)" << endl; break;
			case Auxiliary::AT_SECURE:            stream << "SECURE (Boolean, was exec setuid-like?): " << v.a_val << endl; break;
			case Auxiliary::AT_BASE_PLATFORM:     stream << "BASE_PLATFORM (String identifying real platforms): " << reinterpret_cast<const char *>(v.a_ptr) << endl; break;
			case Auxiliary::AT_RANDOM:            stream << "RANDOM (Address of 16 random bytes): 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_HWCAP2:            stream << "HWCAP2 (More machine-dependent hints about processor capabilities): 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_EXECFN:            stream << "EXECFN (Filename of executable): " << reinterpret_cast<const char *>(v.a_ptr) << endl; break;
			case Auxiliary::AT_SYSINFO:           stream << "SYSINFO (Entry point to the system call function in the vDSO): 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_SYSINFO_EHDR:      stream << "SYSINFO_EHDR (Pointer to the vDSO in memory): 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_L1I_CACHESHAPE:    stream << "L1I_CACHESHAPE: 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_L1D_CACHESHAPE:    stream << "L1D_CACHESHAPE: 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_L2_CACHESHAPE:     stream << "L2_CACHESHAPE: 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_L3_CACHESHAPE:     stream << "L3_CACHESHAPE: 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_L1I_CACHESIZE:     stream << "L1I_CACHESIZE (L1 instruction cache size): " << v.a_val << endl; break;
			case Auxiliary::AT_L1I_CACHEGEOMETRY: stream << "L1I_CACHEGEOMETRY (Geometry of the L1 instruction cache): 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_L1D_CACHESIZE:     stream << "L1D_CACHESIZE (L1 data cache size): " << v.a_val << endl; break;
			case Auxiliary::AT_L1D_CACHEGEOMETRY: stream << "L1D_CACHEGEOMETRY (Geometry of the L1 data cache): 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_L2_CACHESIZE:      stream << "L2_CACHESIZE (L2 cache size): " << v.a_val << endl; break;
			case Auxiliary::AT_L2_CACHEGEOMETRY:  stream << "L2_CACHEGEOMETRY (Geometry of the L2 cache): 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_L3_CACHESIZE:      stream << "L3_CACHESIZE (L3 cache size): " << v.a_val << endl; break;
			case Auxiliary::AT_L3_CACHEGEOMETRY:  stream << "L3_CACHEGEOMETRY (Geometry of the L3 cache): 0x" << v.a_ptr << endl; break;
			case Auxiliary::AT_MINSIGSTKSZ:       stream << "MINSIGSTKSZ (Stack needed for signal delivery): " << v.a_val << endl; break;
			default:                              stream << "[Type " << auxv[auxc].a_type << "]: 0x" << v.a_ptr << endl; break;
		}
	} while (auxv[auxc++].a_type != Auxiliary::AT_NULL);
	stream << endl;
}

void Loader::show_init_stack(uintptr_t entry, uintptr_t stack_pointer, const char ** envp) const {
	if (config.show_args) {
		cerr << "Arguments for 0x" << reinterpret_cast<void*>(entry) << " (with stackpointer at 0x" << reinterpret_cast<void*>(stack_pointer) << "):" << endl;
		show_args(cerr, stack_pointer);
	}
	if (config.show_env) {
		cerr << "Environment variables (at " << reinterpret_cast<void*>(envp) << "):" << endl;
		show_env(cerr, envp);
	}
	if (config.show_auxv) {
		cerr << "Auxiliary vectors:" << endl;
		show_auxv(cerr, envp);
	}

	// Log
	if (LOG.visible(Log::INFO)) {
		LOG_INFO << "Arguments for " << reinterpret_cast<void*>(entry) << " (with stackpointer " << reinterpret_cast<void*>(stack_pointer) << "):" << endl;
		show_args(LOG.append(), stack_pointer);
	}
	if (LOG.visible(Log::TRACE)) {
		LOG_TRACE << "Environment variables (at " << reinterpret_cast<void*>(envp) << "):" << endl;
		show_env(LOG.append(), envp);

		LOG_TRACE << "Auxiliary vectors:" << endl;
		show_auxv(LOG.append(), envp);
	}
}

bool Loader::run(ObjectIdentity * file, const Vector<const char *> & args, uintptr_t stack_pointer, size_t stack_size, const char * entry_point) {
	assert(file != nullptr);
	target = file;
	Object * start = file->current;
	assert(start != nullptr);
	assert(start->file_previous == nullptr);

	if (start->memory_map.empty())
		return false;

	// Executables (and dyn. Objects) will be initialized in _start automatically
	if (start->header.type() != Elf::ET_REL)
		file->flags.initialized = 1;

	// Prepare execution
	Process p(stack_pointer, stack_size);

	// TODO: Should not be hard coded...
	p.aux[Auxiliary::AT_PHDR] = static_cast<long>(start->base + start->header.e_phoff);
	p.aux[Auxiliary::AT_PHNUM] = start->header.e_phnum;

	p.init(args);
	this->argc = p.argc;
	this->argv = p.argv;
	this->envp = p.envp;

	// Binary has to be first in list
	lookup.extract(file);
	lookup.push_front(file);

	// Prepare libraries
	if (!prepare(start)) {
		LOG_ERROR << "Preparation for execution of " << file << " failed..." << endl;
		file->flags.initialized = false;
		return false;
	}

	// Dump contents of initial stack
	auto entry = get_entry_point(start, entry_point);
	show_init_stack(entry, p.stack_pointer, p.envp);
	// Start
	process_started = true;
	p.start(entry);

	return true;
}


bool Loader::run(ObjectIdentity * file, uintptr_t stack_pointer, const char * entry_point) {
	assert(file != nullptr);
	target = file;
	Object * start = file->current;
	assert(start != nullptr);
	assert(start->file_previous == nullptr);

	if (start->memory_map.empty())
		return false;

	// Executed binary will be initialized in _start automatically
	file->flags.initialized = 1;

	this->argc = *reinterpret_cast<int *>(stack_pointer);
	this->argv = reinterpret_cast<const char **>(stack_pointer) + 1;
	this->envp = this->argv + this->argc + 1;

	/* Reorder environment variables
	 * by move empty [consumed] entries (nulled name) to the end
	 * since glibc getenv will stop iterating on such occurences
	 * (and we don't want to change the auxiliary vectors)
	 */
	 {
		// Count entries
		size_t envc = 0;
		while (this->envp[envc] != nullptr)
			envc++;
		// swap each empty entry with one from the end
		for (size_t curr = 0; curr < envc; curr++)
			if (*(this->envp[curr]) == '\0') {
				while (*(this->envp[--envc]) == '\0' && envc > curr) {}
				const char * tmp = this->envp[envc];
				this->envp[envc] = this->envp[curr];
				this->envp[curr] = tmp;
			}
	 }

	// Binary has to be first in list
	lookup.extract(file);
	lookup.push_front(file);

	// Prepare libraries
	if (!prepare(start)) {
		LOG_ERROR << "Preparation for execution of " << file << " failed..." << endl;
		file->flags.initialized = false;
		return false;
	}

	// Dump contents of initial stack
	auto entry = get_entry_point(start, entry_point);
	show_init_stack(entry, stack_pointer, this->envp);
	// Start
	process_started = true;
	Process::start(entry, stack_pointer, this->envp);

	return true;
}


Optional<VersionedSymbol> Loader::resolve_symbol(const char * name, uint32_t hash, uint32_t gnu_hash, const VersionedSymbol::Version & version, namespace_t ns, const ObjectIdentity * obj, ResolveSymbolMode mode) const {
	assert(obj != nullptr || mode == RESOLVE_DEFAULT);
	Optional<VersionedSymbol> best_result = {};

	// Search in calling object (e.g. for deep_bind)
	if (mode == RESOLVE_OBJECT_FIRST) {
		assert(obj->current != nullptr);
		if (obj->current->has_symbol(name, hash, gnu_hash, version, best_result))
			return best_result;
	}

	// Check global scope
	bool after = mode == RESOLVE_AFTER_OBJECT;
	for (const auto & object_file : lookup)
		// but only globals in namespace
		if (object_file.ns == ns && object_file.flags.bind_global == 1 && object_file.current != nullptr) {
			// Skip objects
			if (after) {
				if (obj == &object_file)
					after = false;
			} else if (mode != RESOLVE_EXCEPT_OBJECT || obj != &object_file) {
				assert(object_file.current != nullptr);
				if (object_file.current->has_symbol(name, hash, gnu_hash, version, best_result))
					return best_result;
			}
		}

	if (obj != nullptr) {
		// Seach in dependencies
		if (mode != RESOLVE_NO_DEPENDENCIES) {
			assert(obj->current != nullptr);
			for (const auto & object_file : obj->current->dependencies) {
				assert(object_file != nullptr);
				if (mode != RESOLVE_AFTER_OBJECT && object_file->ns == ns && object_file->flags.bind_global == 1) {
					// Already checked in global scope
					continue;
				} else if (mode != RESOLVE_EXCEPT_OBJECT || obj != object_file) {
					assert(object_file->current != nullptr);
					if (object_file->current->has_symbol(name, hash, gnu_hash, version, best_result))
						return best_result;
				}
			}
		}
		// Search in file (if not done yet)
		if (mode != RESOLVE_OBJECT_FIRST) {
			assert(obj->current != nullptr);
			if (obj->current->has_symbol(name, hash, gnu_hash, version, best_result))
				return best_result;
		}
	}

	// return our best result (first weak symbol)
	return best_result;
}


Optional<VersionedSymbol> Loader::resolve_symbol(uintptr_t addr, namespace_t ns) const {
	Object * o = resolve_object(addr, ns);
	if (o != nullptr)
		return o->resolve_symbol(addr);
	return {};
}


Object * Loader::resolve_object(uintptr_t addr, namespace_t ns) const {
	for (const auto & object_file : lookup) {
		if (object_file.ns != ns)
			continue;
		for (Object * o = object_file.current; o != nullptr; o = o->file_previous) {
			for (const auto & mem : o->memory_map) {
				if (addr >= mem.target.address() && addr <= mem.target.address() + mem.target.size)
					return o;
			}
		}
	}
	return nullptr;
}


uintptr_t Loader::next_address(size_t size) const {
	uintptr_t start = 0;
	uintptr_t end = 0;
	uintptr_t next = next_library_address;
	for (const auto & object_file : lookup)
		for (Object * obj = object_file.current; obj != nullptr; obj = obj->file_previous)
			if (obj->memory_range(start, end) && end > next && (BASEADDRESS < LIBADDRESS || end < BASEADDRESS))
				next = end;

	// Default address
	if (next == 0) {
		next = LIBADDRESS;
	}
	next = Math::align_up(next, Page::SIZE);
	next_library_address = Math::align_up(next + size, Page::SIZE);
	return next;
}

void Loader::reset_address(uintptr_t addr) const {
	next_library_address = Math::min(next_library_address, Math::align_up(addr, Page::SIZE));
}

bool Loader::is_loaded(const ObjectIdentity * ptr) const {
	for (const auto & object_file : lookup)
		if (ptr == &object_file)
			return true;
	return false;
}


Loader * Loader::instance() {
	/* Callbacks like dladdr cannot determine the current instance theirself - hence we need this function.
	   Currently, we only support a single instance & its dead simple,
	   but maybe in the future we can use PID to match the correct one
	*/
	return _instance;
}


bool Loader::start_handler_threads() {
	bool success = true;

	if (filemodification_inotifyfd != -1) {
		Syscall::close(filemodification_inotifyfd);
		filemodification_inotifyfd = -1;
	}
	if (userfaultfd != -1) {
		Syscall::close(userfaultfd);
		userfaultfd = -1;
	}
	if (config.dynamic_update) {
		if (config.detect_outdated == Loader::Config::DETECT_OUTDATED_VIA_USERFAULTFD) {
			if (auto userfault = Syscall::userfaultfd(O_CLOEXEC | O_NONBLOCK)) {
				userfaultfd = userfault.value();
				uffdio_api api;
				auto ioctl = Syscall::ioctl(userfaultfd, UFFDIO_API, &api);
				if (ioctl.failed()) {
					LOG_ERROR << "Enabling userfault failed: " << ioctl.error_message() << endl;
					success = false;
				}
			} else {
				LOG_ERROR << "Initializing userfaultfd failed: " << userfault.error_message() << endl;
				success = false;
			}
		}

		if (auto inotify = Syscall::inotify_init(IN_CLOEXEC | IN_NONBLOCK)) {
			filemodification_inotifyfd = inotify.value();
			for (auto & object_file : lookup)
				object_file.watch(true, false);

			if ((handler_thread = Thread::create(&kickoff_helper_loop, this, true, true, true)) == nullptr) {
				LOG_ERROR << "Creating (file modification) handler thread failed" << endl;
				success = false;
			} else {
				LOG_INFO << "Created (file modification) handler thread" << endl;
			}
		} else {
			LOG_ERROR << "Initializing file modification failed: " << inotify.error_message() << endl;
			success = false;
		}
	} else {
		LOG_INFO << "Not starting file modification handler thread since there are no dynamic updates" << endl;
	}

	return success;
}
