#include "loader.hpp"

#include <dlh/stream/output.hpp>
#include <dlh/stream/buffer.hpp>
#include <dlh/auxiliary.hpp>
#include <dlh/syscall.hpp>
#include <dlh/xxhash.hpp>
#include <dlh/error.hpp>
#include <dlh/file.hpp>
#include <dlh/log.hpp>

#include "object/base.hpp"
#include "compatibility/gdb.hpp"
#include "compatibility/glibc/rtld/dl.hpp"
#include "compatibility/glibc/rtld/global.hpp"
#include "process.hpp"

static Loader * _instance = nullptr;

int observer_kickoff(void * ptr) {
	reinterpret_cast<Loader *>(ptr)->observer();
	return 0;
}

Loader::Loader(uintptr_t luci_self, const char * sopath, bool dynamicUpdate) : dynamic_update(dynamicUpdate), next_namespace(NAMESPACE_BASE + 1) {
	if (dynamic_update) {
		if (auto inotify = Syscall::inotify_init(IN_CLOEXEC)) {
			inotifyfd = inotify.value();
			if (Thread::create(&observer_kickoff, this, true) == nullptr) {
				LOG_ERROR << "Creating background thread failed" << endl;
			} else {
				LOG_INFO << "Created observer background thread";
			}
		} else {
			LOG_ERROR << "Initializing inotify failed: " << inotify.error_message() << endl;
		}
	}

	// Add vDSO (if available)
	if (auto vdso = Auxiliary::vector(Auxiliary::AT_SYSINFO_EHDR))
		open(vdso.value(), true, true, true, "linux-vdso.so.1");

	// add self as dynamic exec
	assert(luci_self != 0);
	self = open(luci_self, true, true, true, sopath, NAMESPACE_BASE, Elf::ET_DYN);
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

static void observer_signal(int signum) {
	LOG_INFO << "Observer ends (Signal " << signum << ")" << endl;
	Syscall::exit(EXIT_SUCCESS);
}

void Loader::observer() {
	// Set signal handler
	struct sigaction action;
	Memory::set(&action, 0, sizeof(struct sigaction));
	action.sa_handler = observer_signal;

	if (auto sigaction = Syscall::sigaction(SIGTERM, &action, NULL); sigaction.failed())
		LOG_WARNING << "Unable to set observer signal handler: " << sigaction.error_message() << endl;

	if (auto prctl = Syscall::prctl(PR_SET_PDEATHSIG, SIGTERM); prctl.failed())
		LOG_WARNING << "Unable to set observer death signal: " << prctl.error_message() << endl;

	// Loop over inotify
	char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
	while (true) {
		auto read = Syscall::read(inotifyfd, buf, sizeof(buf));

		if (read.failed() && read.error() != EAGAIN) {
			LOG_ERROR << "Reading file modifications failed: " << read.error_message() << endl;
			break;
		}

		ssize_t len = read.value();
		if (len <= 0)
			break;

		char *ptr = buf;
		while (ptr < buf + len) {
			const struct inotify_event * event = reinterpret_cast<const struct inotify_event *>(ptr);
			ptr += sizeof(struct inotify_event) + event->len;

			// Get Object
			mutex.lock();
			for (auto & object_file : lookup)
				if (event->wd == object_file.wd) {
					LOG_DEBUG << "Possible file modification in " << object_file.path << endl;
					assert((event->mask & IN_ISDIR) == 0);
					if (object_file.load() != nullptr)
						prepare(true);
					break;
				}
			mutex.unlock();
		}
	}
	LOG_INFO << "Observer background thread ends." << endl;
}


ObjectIdentity * Loader::library(const char * filename, const Vector<const char *> & rpath, const Vector<const char *> & runpath, namespace_t ns)  {
	StrPtr path(filename);
	auto name = path.find_last('/');
	if (ns != NAMESPACE_NEW)
		for (auto & i : lookup)
			if (ns == i.ns && name == i.name)
				return &i;

	ObjectIdentity * lib;
	// only file name provided - look for it in search paths
	if (path == name) {
		for (const auto & path : { rpath, library_path_runtime, runpath, library_path_config, library_path_default }) {
			for (auto & dir : path)
				if ((lib = open(filename, dir, ns)) != nullptr)
					return lib;
		}


	} else {
		// Full path
		if ((lib = open(filename, ns)) != 0)
			return lib;
	}

	LOG_ERROR << "Library '" << filename << "' cannot be found" << endl;
	return nullptr;
}

ObjectIdentity * Loader::open(const char * filename, const char * directory, namespace_t ns) {
	auto filename_len = String::len(filename);
	auto directory_len = String::len(directory);
	char path[directory_len + filename_len + 2];
	String::copy(path, directory, directory_len);
	path[directory_len] = '/';
	String::copy(path + directory_len + 1, filename, filename_len + 1);

	return open(path, ns);
}

ObjectIdentity * Loader::open(const char * filepath, namespace_t ns) {
	// Does file contain a valid full path?
	if (File::exists(filepath)) {
		if (ns == NAMESPACE_NEW)
			ns = next_namespace++;

		LOG_DEBUG << "Loading " << filepath << "..." << endl;
		auto i = lookup.emplace_back(*this, filepath, ns);
		assert(i);
		if (i->load() != nullptr) {
			return i.operator->();
		} else {
			LOG_ERROR << "Unable to open " << filepath << endl;
			lookup.pop_back();
		}
	}
	return nullptr;
}

ObjectIdentity * Loader::open(uintptr_t addr, bool prevent_updates, bool is_prepared, bool is_mapped, const char * filepath, namespace_t ns, Elf::ehdr_type type) {
	if (ns == NAMESPACE_NEW)
		ns = next_namespace++;

	LOG_DEBUG << "Loading from memory " << (void*)addr << " (" << filepath << ")..." << endl;
	auto i = lookup.emplace_back(*this, filepath, ns);

	if (prevent_updates) {
		i->flags.updatable = 0;
		i->flags.immutable_source = 1;
	}

	Object * o = i->load(addr, !is_prepared, !is_mapped, type);
	if (o != nullptr) {
		if (is_prepared) {
			i->flags.initialized = 1;
			o->status = Object::STATUS_PREPARED;
		}
		if (is_mapped) {
			i->base = o->base = addr;
			for (const auto & segment : o->segments)
				if (segment.type() == Elf::PT_DYNAMIC) {
					i->dynamic = (reinterpret_cast<const Elf::Header *>(addr)->type() != Elf::ET_EXEC ? i->base : 0) + segment.virt_addr();
					break;
				}
		}
		return i.operator->();
	} else {
		LOG_ERROR << "Unable to open " << (void*)addr << endl;
		lookup.pop_back();
	}
	return nullptr;
}

ObjectIdentity * Loader::dlopen(const char * file, namespace_t ns) {
	GDB::notify(GDB::State::RT_ADD);
	auto o = library(file, {}, {}, ns);
	if (o != nullptr && !o->prepare()) {
		LOG_WARNING << "Preparation of " << o << " failed!" << endl;
		o = nullptr;
	}
	GDB::notify(GDB::State::RT_CONSISTENT);
	if (o != nullptr && !o->initialize()){
		LOG_WARNING << "Initialization of " << o << " failed!" << endl;
		o = nullptr;
	}
	return o;
}

extern uintptr_t __stack_chk_guard;
bool Loader::prepare(bool update) {
	// Init GLIBC globals
	GLIBC::RTLD::init_globals(*this);

	// Prepare
	for (auto & o : reverse(lookup))
		if (!o.prepare())
			return false;

	// Optional: Update relocations
	if (update) {
		// TODO: Unmap if relro
		for (auto & o : reverse(lookup))
			o.current->update();
	}

	/*
	// Protect memory (TODO: for relro)
	for (auto & o : reverse(lookup))
		if (!o.current->protect())
			return false;
	*/

	// Initialize TLS
	main_thread = tls.allocate(nullptr, true);
	auto random = Auxiliary::vector(Auxiliary::AT_RANDOM);
	if (random.valid()) {
		main_thread->setup_guards(random.a_un.a_ptr);
		__stack_chk_guard = main_thread->stack_guard;
		LOG_DEBUG << "Stack Guard: " << (void*)main_thread->stack_guard << endl;
		LOG_DEBUG << "Pointer Guard: " << (void*)main_thread->pointer_guard << endl;
	} else {
		LOG_WARNING << "No AT_RANDOM!" << endl;
	}
	tls.dtv_setup(main_thread);

	// Initialize GDB
	GDB::init(*this);
	GDB::notify();

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
	lookup.front().filename = ""; // required by gdb

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

	// Binary has to be first in list
	lookup.extract(file);
	lookup.push_front(file);
	lookup.front().filename = ""; // required by gdb

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

Optional<VersionedSymbol> Loader::resolve_symbol(const char * name, uint32_t hash, uint32_t gnu_hash, const VersionedSymbol::Version & version, namespace_t ns, const ObjectIdentity * after) const {
	Optional<VersionedSymbol> f = {};
	for (const auto & object_file : lookup)
		if (object_file.ns == ns && object_file.current != nullptr) {
			// Skip objects
			if (after != nullptr) {
				if (after == &object_file)
					after = nullptr;
				continue;
			}

			// Only compare to latest version
			auto obj = object_file.current;
			assert(obj != nullptr);
			auto s = obj->resolve_symbol(name, hash, gnu_hash, version);
			if (s) {
				assert(s->valid());
				assert(s->bind() != Elf::STB_LOCAL); // should not be returned
				if (s->bind() != Elf::STB_WEAK)
					return s;
				else if (!f)
					f = s;
			}
		}
	return f;
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
			if (obj->memory_range(start, end) && end > next)
				next = end;

	// Default address
	if (next == 0) {
		next = 0x500000;
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
