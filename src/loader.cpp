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
#include "process.hpp"

int observer_kickoff(void * ptr) {
	reinterpret_cast<Loader *>(ptr)->observer();
	return 0;
}

Loader::Loader(const char * path, bool dynamicUpdate) : dynamic_update(dynamicUpdate) {
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
		open(vdso.a_un.a_ptr, true, true);

	// add self
	//auto self = Utils::aux(Auxiliary::AT_BASE);
	//if (self.valid())
	//	open(self.a_un.a_ptr), true, true, path);
}

Loader::~Loader() {
	if (dynamic_update) {
		if (close(inotifyfd) != 0) {
			LOG_ERROR << "Closing inotify failed: " << strerror(errno) << endl;
		} else {
			LOG_INFO << "Destroyed observer background thread" << endl;
		}
	}

	lookup.clear();
}


void Loader::observer() {
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
					if (object_file.open() != nullptr)
						prepare();
					break;
				}
			mutex.unlock();
		}
	}
	LOG_INFO << "Observer background thread ends." << endl;
}


ObjectIdentity * Loader::library(const char * filename, const Vector<const char *> & rpath, const Vector<const char *> & runpath, DL::Lmid_t ns)  {
	StrPtr path(filename);
	auto name = path.rchr('/');
	if (ns != DL::LM_ID_NEWLN)
		for (auto & i : lookup)
			if (ns == i.ns && name == i.name)
				return &i;

	ObjectIdentity * lib;
	// only file name provided - look for it in search paths
	if (path == name) {
		for (const Vector<const char *> & path : { rpath, library_path_runtime, runpath, library_path_config, library_path_default }) {
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

ObjectIdentity * Loader::open(const char * filename, const char * directory, DL::Lmid_t ns) {
	auto filename_len = strlen(filename);
	auto directory_len = strlen(directory);
	char path[directory_len + filename_len + 2];
	::strncpy(path, directory, directory_len);
	path[directory_len] = '/';
	::strncpy(path + directory_len + 1, filename, filename_len + 1);

	return open(path, ns);
}

ObjectIdentity * Loader::open(const char * filepath, DL::Lmid_t ns) {
	// Does file contain a valid full path?
	if (File::exists(filepath)) {
		if (ns == DL::LM_ID_NEWLN)
			ns = get_new_ns();

		LOG_DEBUG << "Loading " << filepath << "..." << endl;
		auto i = lookup.emplace_back(*this, filepath, ns);
		assert(i);
		if (i->open() != nullptr) {
			return i.operator->();
		} else {
			LOG_ERROR << "Unable to open " << filepath << endl;
			lookup.pop_back();
		}
	}
	return nullptr;
}

ObjectIdentity * Loader::open(void * ptr, bool prevent_updates, bool in_execution, const char * filepath, DL::Lmid_t ns) {
	if (ns == DL::LM_ID_NEWLN)
		ns = get_new_ns();

	LOG_DEBUG << "Loading from memory " << (void*)ptr << " (" << filepath << ")..." << endl;
	auto i = lookup.emplace_back(*this, filepath, ns);

	if (prevent_updates) {
		i->flags.updatable = 0;
		i->flags.immutable_source = 1;
	}

	Object * o = i->open(ptr, !in_execution);
	if (o != nullptr) {
		if (in_execution) {
			i->flags.initialized = 1;
			o->base = reinterpret_cast<uintptr_t>(ptr);
			o->is_prepared = true;
			o->is_protected = true;
		}
		return i.operator->();
	} else {
		LOG_ERROR << "Unable to open " << ptr << endl;
		lookup.pop_back();
	}
	return nullptr;
}

DL::Lmid_t Loader::get_new_ns() const {
	DL::Lmid_t ns = DL::LM_ID_BASE + 1;
	for (const auto & i : lookup)
		if (i.ns >= ns)
			ns = i.ns + 1;
	return ns;
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

bool Loader::run(ObjectIdentity * file, Vector<const char *> args, uintptr_t stack_pointer, size_t stack_size) {
	assert(file != nullptr);
	Object * start = file->current;
	assert(start != nullptr);
	assert(start->file_previous == nullptr);

	if (start->memory_map.size() == 0)
		return false;

	// Executed binary will be initialized in _start automatically
	file->flags.initialized = 1;

	// Prepare libraries
	if (!prepare()) {
		LOG_ERROR << "Preparation for execution of " << file << " failed..." << endl;
		file->flags.initialized = false;
		return false;
	}

	// Prepare execution
	Process p(stack_pointer, stack_size);

	// TODO: Should not be hard coded...
	p.aux[Auxiliary::AT_PHDR] = start->base + start->header.e_phoff;
	p.aux[Auxiliary::AT_PHNUM] = start->header.e_phnum;

	args.push_front(start->file.path.str);
	p.init(args);

	uintptr_t entry = start->header.entry();
	LOG_INFO << "Start at " << (void*)start->base << " + " << (void*)(entry) << endl;
	p.start(start->base + entry);

	return true;
}

Optional<VersionedSymbol> Loader::resolve_symbol(const VersionedSymbol & sym, DL::Lmid_t ns) const {
	for (const auto & object_file : lookup)
		if (object_file.ns == ns && object_file.current != nullptr) {
			// Only compare to latest version
			auto obj = object_file.current;
			assert(obj != nullptr);
			auto s = obj->resolve_symbol(sym);
			if (s) {
				assert(s->valid());
				return s;
			}
		}
	return { };
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
