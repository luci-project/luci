#include "loader.hpp"

#include <dlh/stream/output.hpp>
#include <dlh/stream/buffer.hpp>
#include <dlh/auxiliary.hpp>
#include <dlh/syscall.hpp>
#include <dlh/xxhash.hpp>
#include <dlh/error.hpp>
#include <dlh/file.hpp>
#include <dlh/log.hpp>

#include "compatibility/glibc/rtld/global.hpp"
#include "compatibility/glibc/rtld/dl.hpp"
#include "compatibility/gdb.hpp"
#include "object/base.hpp"
#include "process.hpp"


static Loader * _instance = nullptr;

void* kickoff_observer(void * ptr) {
	reinterpret_cast<Loader *>(ptr)->observer_loop();
	return nullptr;
}

Loader::Loader(uintptr_t luci_self, const char * sopath, bool dynamicUpdate, bool dynamicDlUpdate, bool dynamicWeak)
 : dynamic_update(dynamicUpdate), dynamic_dlupdate(dynamicUpdate && dynamicDlUpdate), dynamic_weak(dynamicWeak), next_namespace(NAMESPACE_BASE + 1) {
	default_flags.bind_global = 1;

	if (dynamic_update) {
		default_flags.updatable = 1;
		if (auto inotify = Syscall::inotify_init(IN_CLOEXEC)) {
			inotifyfd = inotify.value();
			if (Thread::create(&kickoff_observer, this, true) == nullptr) {
				LOG_ERROR << "Creating inotify observer background thread failed" << endl;
			} else {
				LOG_INFO << "Created observer background thread" << endl;
			}
		} else {
			LOG_ERROR << "Initializing inotify failed: " << inotify.error_message() << endl;
		}
	} else {
		default_flags.immutable_source = 1;
	}

	// Flags for vDSO and Luci
	ObjectIdentity::Flags static_flags;
	static_flags.bind_global = 1;
	static_flags.persistent = 1;
	static_flags.immutable_source = 1;
	static_flags.initialized = 1;
	static_flags.premapped = 1;

	// Add vDSO (if available)
	if (auto vdso = Auxiliary::vector(Auxiliary::AT_SYSINFO_EHDR)) {
		if (open("linux-vdso.so.1", static_flags, NAMESPACE_BASE, vdso.value()) == nullptr)
			LOG_WARNING << "Unable to load vDSO!" << endl;
	}

	// add self as dynamic exec
	assert(luci_self != 0);
	if ((self = open(sopath, static_flags, NAMESPACE_BASE, luci_self)) == nullptr) {
		LOG_ERROR << "Unable to load Luci (" << sopath << ")..." << endl;
		assert(false);
	}
	LOG_INFO << "Base of " << sopath << " is " << (void*)(self->current->base) << endl;
	self->current->base = 0;

	// Assign current instance
	_instance = this;
}


Loader::~Loader() {
	_instance = nullptr;

	if (dynamic_update) {
		if (auto close = Syscall::close(inotifyfd)) {
			LOG_INFO << "Destroyed observer background thread" << endl;
		} else {
			LOG_ERROR << "Closing inotify failed: " << close.error_message() << endl;
		}
	}

	lookup.clear();
}


ObjectIdentity * Loader::library(const char * filename, ObjectIdentity::Flags flags, const Vector<const char *> & rpath, const Vector<const char *> & runpath, namespace_t ns, bool load) {
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
				for (auto & dir : path)
					if ((lib = open(filename, dir, flags, ns)) != nullptr)
						return lib;
			}
		} else {
			// Full path
			if ((lib = open(filename, flags, ns)) != 0)
				return lib;
		}

		LOG_ERROR << "Library '" << filename << "' cannot be found" << endl;
	}
	return nullptr;
}


ObjectIdentity * Loader::open(const char * filename, const char * directory, ObjectIdentity::Flags flags, namespace_t ns) {
	auto filename_len = String::len(filename);
	auto directory_len = String::len(directory);
	char path[directory_len + filename_len + 2];
	String::copy(path, directory, directory_len);
	path[directory_len] = '/';
	String::copy(path + directory_len + 1, filename, filename_len + 1);

	return open(path, flags, ns);
}


ObjectIdentity * Loader::open(const char * filepath, ObjectIdentity::Flags flags, namespace_t ns, uintptr_t addr, Elf::ehdr_type type) {
	// Does file contain a valid full path or do we have a memory address?
	if (addr != 0 || File::exists(filepath)) {
		if (ns == NAMESPACE_NEW)
			ns = next_namespace++;

		LOG_DEBUG << "Loading " << filepath;
		if (addr != 0)
			LOG_DEBUG_APPEND << " from memory at " << (void*)addr;
		LOG_DEBUG_APPEND << "..." << endl;

		GDB::notify(GDB::RT_ADD);
		auto i = lookup.emplace_back(*this, flags, filepath, ns, nullptr);
		assert(i);

		auto o = i->load(addr, type);
		GDB::notify(GDB::RT_CONSISTENT);
		if (o != nullptr) {
			return i.operator->();
		} else {
			LOG_ERROR << "Unable to open " << filepath << endl;
			lookup.pop_back();
		}
	}
	return nullptr;
}


ObjectIdentity * Loader::dlopen(const char * file, ObjectIdentity::Flags flags, namespace_t ns, bool load) {
	ObjectIdentity * o = file == nullptr ? target : library(file, flags, {}, {}, ns, load);
	if (o != nullptr) {
		// Update flags
		o->flags.bind_now = flags.bind_now;  // TODO: if change to true, resolve all relocations
		o->flags.bind_global = flags.bind_global;
		o->flags.bind_deep = flags.bind_deep;
		o->flags.persistent = flags.persistent;
		o->flags.updatable = dynamic_dlupdate;

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
		// TODO: Unmap if relro
		for (auto & o : reverse(lookup))
			if (!o.update())
				return false;
		// update dlsym trampolines
		if (dynamic_dlupdate)
			dlsyms.update();
	}

	// Protect memory (TODO: for relro)
	for (auto & o : reverse(lookup))
		if (!o.protect())
			return false;

	return true;
}


extern uintptr_t __stack_chk_guard;
bool Loader::prepare() {
	// Init GLIBC globals
	GLIBC::RTLD::init_globals(*this);

	// Relocate all libraries
	if (!relocate(false))
		return false;

	// Initialize TLS
	main_thread = tls.allocate(nullptr, true);
	auto random = Auxiliary::vector(Auxiliary::AT_RANDOM);
	if (random.valid()) {
		main_thread->setup_guards(random.a_un.a_ptr);
		// TODO: __stack_chk_guard = main_thread->stack_guard;
		LOG_DEBUG << "Stack Guard: " << (void*)main_thread->stack_guard << endl;
		LOG_DEBUG << "Pointer Guard: " << (void*)main_thread->pointer_guard << endl;
	} else {
		LOG_WARNING << "No AT_RANDOM!" << endl;
	}
	tls.dtv_setup(main_thread);

	// Initialize GDB
	GDB::init(*this);

	// Initialize (but not executable itself)
	for (auto & o : reverse(lookup))
		if (!o.initialize())
			return false;

	// Mark start of program
	GLIBC::RTLD::starting();

	return true;
}


bool Loader::run(ObjectIdentity * file, const Vector<const char *> & args, uintptr_t stack_pointer, size_t stack_size) {
	assert(file != nullptr);
	target = file;
	Object * start = file->current;
	assert(start != nullptr);
	assert(start->file_previous == nullptr);

	if (start->memory_map.size() == 0)
		return false;

	// Executed binary will be initialized in _start automatically
	file->flags.initialized = 1;

	// Prepare execution
	Process p(stack_pointer, stack_size);

	// TODO: Should not be hard coded...
	p.aux[Auxiliary::AT_PHDR] = start->base + start->header.e_phoff;
	p.aux[Auxiliary::AT_PHNUM] = start->header.e_phnum;

	p.init(args);
	this->argc = p.argc;
	this->argv = p.argv;
	this->envp = p.envp;

	// Binary has to be first in list
	lookup.extract(file);
	lookup.push_front(file);

	// Prepare libraries
	if (!prepare()) {
		LOG_ERROR << "Preparation for execution of " << file << " failed..." << endl;
		file->flags.initialized = false;
		return false;
	}

	// Start
	uintptr_t entry = start->header.entry();
	LOG_INFO << "Start at " << (void*)start->base << " + " << (void*)(entry) << endl;
	p.start(start->base + entry);

	return true;
}


bool Loader::run(ObjectIdentity * file, uintptr_t stack_pointer) {
	assert(file != nullptr);
	target = file;
	Object * start = file->current;
	assert(start != nullptr);
	assert(start->file_previous == nullptr);

	if (start->memory_map.size() == 0)
		return false;

	// Executed binary will be initialized in _start automatically
	file->flags.initialized = 1;

	this->argc = *reinterpret_cast<int *>(stack_pointer);;
	this->argv = reinterpret_cast<const char **>(stack_pointer) + 1;
	this->envp = this->argv + this->argc + 1;

	// Reorder environment variables (skip empty [consumed] entries)
	{
		size_t curr = 0;
		for (size_t e = 0; this->envp[e] != nullptr; e++)
			if (*(this->envp[curr] = this->envp[e]) != '\0')
				curr++;
		this->envp[curr] = nullptr;
	}

	// Binary has to be first in list
	lookup.extract(file);
	lookup.push_front(file);

	// Prepare libraries
	if (!prepare()) {
		LOG_ERROR << "Preparation for execution of " << file << " failed..." << endl;
		file->flags.initialized = false;
		return false;
	}

	// Start
	uintptr_t entry = start->header.entry();
	LOG_INFO << "Start at " << (void*)start->base << " + " << (void*)(entry) << " (using existing stack at " << (void*) stack_pointer << ")"<< endl;
	Process::start(start->base + entry, stack_pointer);

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
	auto o = resolve_object(addr, ns);
	if (o != nullptr)
		return o->resolve_symbol(addr);
	return {};
}


Object * Loader::resolve_object(uintptr_t addr, namespace_t ns) const {
	for (const auto & object_file : lookup)
		if (object_file.ns == ns)
			for (auto o = object_file.current; o != nullptr; o = o->file_previous)
				for (const auto & mem : o->memory_map)
					if (addr >= mem.target.address() && addr <= mem.target.address() + mem.target.size)
						return o;
	return nullptr;
}


uintptr_t Loader::next_address() const {
	uintptr_t start = 0, end = 0, next = 0;
	for (const auto & object_file : lookup)
		for (Object * obj = object_file.current; obj != nullptr; obj = obj->file_previous)
			if (obj->memory_range(start, end) && end > next && (BASEADDRESS < LIBADDRESS || end < BASEADDRESS))
				next = end;

	// Default address
	if (next == 0) {
		next = LIBADDRESS;
	}
	return next;
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
