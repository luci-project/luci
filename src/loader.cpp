#include "loader.hpp"

#include <dlh/unistd.hpp>
#include <dlh/errno.hpp>
#include <dlh/stream/output.hpp>
#include <dlh/stream/buffer.hpp>
#include <dlh/utils/auxiliary.hpp>
#include <dlh/utils/xxhash.hpp>
#include <dlh/utils/file.hpp>
#include <dlh/utils/log.hpp>

#include "object/base.hpp"
#include "compatibility/gdb.hpp"
#include "process.hpp"

static Loader * _instance = nullptr;

int observer_kickoff(void * ptr) {
	reinterpret_cast<Loader *>(ptr)->observer();
	return 0;
}

Loader::Loader(void * luci_self, const char * sopath, bool dynamicUpdate) : dynamic_update(dynamicUpdate), next_namespace(NAMESPACE_BASE + 1) {
	errno = 0;

	if (dynamic_update) {
		if ((inotifyfd = ::inotify_init1(IN_CLOEXEC)) == -1) {
			LOG_ERROR << "Initializing inotify failed: " << strerror(errno) << endl;
		} else if (Thread::create(&observer_kickoff, this, true) == nullptr) {
			LOG_ERROR << "Creating background thread failed: " << strerror(errno) << endl;
		} else {
			LOG_INFO << "Created observer background thread";
		}
	}

	// Add vDSO (if available)
	auto vdso = Auxiliary::vector(Auxiliary::AT_SYSINFO_EHDR);
	if (vdso.valid())
		open(vdso.a_un.a_ptr, true, true, true, "linux-vdso.so.1");

	// add self as dynamic exec
	assert(luci_self != nullptr);
	self = open(luci_self, true, true, true, sopath, NAMESPACE_BASE, Elf::ET_DYN);
	LOG_INFO << "Base of " << sopath << " is " << (void*)(self->current->base) << endl;
	self->current->base = 0;

	// Assign current instance
	_instance = this;
}

Loader::~Loader() {
	_instance = nullptr;

	if (dynamic_update) {
		if (close(inotifyfd) != 0) {
			LOG_ERROR << "Closing inotify failed: " << strerror(errno) << endl;
		} else {
			LOG_INFO << "Destroyed observer background thread" << endl;
		}
	}

	lookup.clear();
}

static void observer_signal(int signum) {
	LOG_INFO << "Observer ends (Signal " << signum << ")" << endl;
	exit(0);
}

void Loader::observer() {
	// Set signal handler
	errno = 0;
	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = observer_signal;
	auto ret_sa = sigaction(SIGTERM, &action, NULL);
	if (ret_sa == -1)
		LOG_WARNING << "Unable to set observer signal handler: " << __errno_string(errno) << endl;

	// Set death handler
	errno = 0;
	auto ret_pr = prctl(PR_SET_PDEATHSIG, SIGTERM);  // Stop with parent
	if (ret_pr == -1)
		LOG_WARNING << "Unable to set observer death signal: " << __errno_string(errno) << endl;

	// Loop over inotify
	char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
	while (true) {
		errno = 0;
		ssize_t len = read(inotifyfd, buf, sizeof(buf));

		if (len == -1 && errno != EAGAIN) {
			LOG_ERROR << "Reading file modifications failed: " << strerror(errno) << endl;
			break;
		} else if (len <= 0) {
			break;
		}

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
						prepare();
					break;
				}
			mutex.unlock();
		}
	}
	LOG_INFO << "Observer background thread ends." << endl;
}


ObjectIdentity * Loader::library(const char * filename, const Vector<const char *> & rpath, const Vector<const char *> & runpath, namespace_t ns)  {
	StrPtr path(filename);
	auto name = path.rchr('/');
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
	auto filename_len = strlen(filename);
	auto directory_len = strlen(directory);
	char path[directory_len + filename_len + 2];
	::strncpy(path, directory, directory_len);
	path[directory_len] = '/';
	::strncpy(path + directory_len + 1, filename, filename_len + 1);

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

ObjectIdentity * Loader::open(void * ptr, bool prevent_updates, bool is_prepared, bool is_mapped, const char * filepath, namespace_t ns, Elf::ehdr_type type) {
	if (ns == NAMESPACE_NEW)
		ns = next_namespace++;

	LOG_DEBUG << "Loading from memory " << (void*)ptr << " (" << filepath << ")..." << endl;
	auto i = lookup.emplace_back(*this, filepath, ns);

	if (prevent_updates) {
		i->flags.updatable = 0;
		i->flags.immutable_source = 1;
	}

	Object * o = i->load(ptr, !is_prepared, !is_mapped, type);
	if (o != nullptr) {
		if (is_prepared) {
			i->flags.initialized = 1;
			o->is_prepared = true;
		}
		if (is_mapped) {
			o->is_protected = true;
			i->base = o->base = reinterpret_cast<uintptr_t>(ptr);
			for (const auto & segment : o->segments)
				if (segment.type() == Elf::PT_DYNAMIC) {
					i->dynamic = (reinterpret_cast<const Elf::Header *>(ptr)->type() != Elf::ET_EXEC ? i->base : 0) + segment.virt_addr();
					break;
				}
		}
		return i.operator->();
	} else {
		LOG_ERROR << "Unable to open " << ptr << endl;
		lookup.pop_back();
	}
	return nullptr;
}

bool Loader::prepare() {
	// Relocate
	LOG_DEBUG << "Relocate..." << endl;
	for (auto & o : lookup) {
		if (o.current->is_prepared)
			o.current->update();
		else if (o.current->prepare())
			o.current->is_prepared = true;
		else
			return false;
	}

	// Protect
	LOG_DEBUG << "Protect memory..." << endl;
	for (auto & o : lookup)
		if (o.current->is_protected)
			continue;
		else if (o.current->protect())
			o.current->is_protected = true;
		else
			return false;

	// Initialize (but not binary itself)
	LOG_DEBUG << "Initialize..." << endl;
	for (auto & o : lookup)
		if (!o.initialize())
			return false;

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

	for (const auto & x : lookup)
		LOG_INFO << " - " << x.filename << endl;

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

	gdb_initialize(*this);

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

	gdb_initialize(*this);

	uintptr_t entry = start->header.entry();
	LOG_INFO << "Start at " << (void*)start->base << " + " << (void*)(entry) << " (using existing stack at " << (void*) stack_pointer << ")"<< endl;
	Process::start(start->base + entry, stack_pointer);

	return true;
}

Optional<VersionedSymbol> Loader::resolve_symbol(const VersionedSymbol & sym, namespace_t ns) const {
	Optional<VersionedSymbol> f = {};
	for (const auto & object_file : lookup)
		if (object_file.ns == ns && object_file.current != nullptr) {
			// Only compare to latest version
			auto obj = object_file.current;
			assert(obj != nullptr);
			auto s = obj->resolve_symbol(sym);
			if (s) {
				assert(s->valid());
				switch (s->bind()) {
					case Elf::STB_GLOBAL:
						return s;
					case Elf::STB_WEAK:
						if (!f)
							f = s;
						break;
					default:
						assert(false);
						continue;
				};
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

Loader * Loader::instance() {
	/* Callbacks like dladdr cannot determine the current instance theirself - hence we need this function.
	   Currently, we only support a single instance & its dead simple,
	   but maybe in the future we can use PID to match the correct one
	*/
	return _instance;
}
