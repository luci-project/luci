#include "loader.hpp"

#include <sys/inotify.h>

#include <utility>

#include "xxhash64.h"

#include "process.hpp"
#include "object.hpp"
#include "generic.hpp"

#include "output.hpp"

void * observer_kickoff(void * ptr) {
	reinterpret_cast<Loader *>(ptr)->observer();
	return nullptr;
}

Loader::Loader(const char * self, bool dynamicUpdate) : dynamic_update(dynamicUpdate) {
	errno = 0;
	if (::pthread_mutex_init(&mutex, nullptr) != 0) {
		LOG_ERROR << "Creating loader mutex failed: " << strerror(errno);
	}
	if (dynamic_update) {
		if ((inotifyfd = ::inotify_init1(IN_CLOEXEC)) == -1)
			LOG_ERROR << "Initializing inotify failed: " << strerror(errno);
		else if (::pthread_create(&observer_thread, nullptr, &observer_kickoff, this) != 0)
			LOG_ERROR << "Creating background thread failed: " << strerror(errno);
		else if (::pthread_detach(observer_thread) != 0)
			LOG_ERROR << "Detaching background thread failed: " << strerror(errno);
		else
			LOG_INFO << "Created observer background thread";
	}
}

Loader::~Loader() {
	if (dynamic_update) {
		if (close(inotifyfd) != 0)
			LOG_ERROR << "Closing inotify failed: " << strerror(errno);
		else
			LOG_INFO << "Destroyed observer background thread";
	}
	if (::pthread_mutex_destroy(&mutex) != 0) {
		LOG_ERROR << "Destroying loader mutex failed: " << strerror(errno);
	}

	lookup.clear();
}


void Loader::observer() {
	char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));

	while (true) {
		errno = 0;
		ssize_t len = read(inotifyfd, buf, sizeof(buf));

		if (len == -1 && errno != EAGAIN) {
			LOG_ERROR << "Reading file modifications failed: " << strerror(errno);
			break;
		} else if (len <= 0) {
			break;
		}

		char *ptr = buf;
		while (ptr < buf + len) {
			const struct inotify_event * event = reinterpret_cast<const struct inotify_event *>(ptr);
			ptr += sizeof(struct inotify_event) + event->len;

			// Get Object
			lock();
			for (auto & object_file : lookup)
				if (event->wd == object_file.wd) {
					LOG_DEBUG << "Possible file modification in " << object_file.path;
					assert((event->mask & IN_ISDIR) == 0);
					if (object_file.load() != nullptr)
						prepare();
					break;
				}
			unlock();
		}
	}
	LOG_INFO << "Observer background thread ends.";
}


ObjectIdentity * Loader::library(const char * filename, const std::vector<const char *> & rpath, const std::vector<const char *> & runpath, DL::Lmid_t ns)  {
	StrPtr path(filename);
	auto name = path.rchr('/');
	if (ns != DL::LM_ID_NEWLN) {
		for (auto & i : lookup)
			if (ns == i.ns && name == i.name)
				return &i;
	}

	ObjectIdentity * lib;
	// only file name provided - look for it in search paths
	if (path == name) {
		for (const std::vector<const char *> & path : { rpath, library_path_runtime, runpath, library_path_config, library_path_default })
			for (auto & dir : path)
				if ((lib = open(filename, dir, ns)) != nullptr)
					return lib;
	} else {
		// Full path
		if ((lib = open(filename, ns)) != 0)
			return lib;
	}

	LOG_ERROR << "Library '" << filename << "' cannot be found";
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
	if (access(filepath, F_OK ) == 0) {
		if (ns == DL::LM_ID_NEWLN) {
			ns = DL::LM_ID_BASE + 1;
			for (const auto & i : lookup)
				if (i.ns >= ns)
					ns = i.ns + 1;
		}

		LOG_DEBUG << "Loading " << filepath << "...";
		ObjectIdentity & i = lookup.emplace_back(*this, filepath, ns);
		if (i.load()) {
			return &i;
		} else {
			LOG_ERROR << "Unable to open " << filepath;
			lookup.pop_back();
		}
	}
	return nullptr;
}

bool Loader::prepare() {
	// Relocate
	LOG_DEBUG << "Relocate...";
	for (auto & o : lookup)
		if (o.current->is_prepared)
			o.current->update();
		else if (o.current->prepare())
			o.current->is_prepared = true;
		else
			return false;

	// Protect
	LOG_DEBUG << "Protect memory...";
	for (auto & o : lookup)
		if (o.current->is_protected)
			continue;
		else if (o.current->protect())
			o.current->is_protected = true;
		else
			return false;

	// Initialize (but not binary itself)
	LOG_DEBUG << "Initialize...";
	for (auto & o : lookup)
		if (!o.initialize())
			return false;

	return true;
}

bool Loader::run(ObjectIdentity * file, std::vector<const char *> args, uintptr_t stack_pointer, size_t stack_size) {
	Object * start = file->current;
	assert(start->file_previous == nullptr);

	if (start->memory_map.size() == 0)
		return false;

	// Executed binary will be initialized in _start automatically
	file->flags.initialized = 1;

	// Prepare libraries
	if (!prepare()) {
		LOG_ERROR << "Preparation for execution of " << file << " failed...";
		file->flags.initialized = false;
		return false;
	}

	// Prepare execution
	Process p(stack_pointer, stack_size);

	// TODO: Should not be hard coded...
	p.aux[Auxiliary::AT_PHDR] = start->base + start->header.e_phoff;
	p.aux[Auxiliary::AT_PHNUM] = start->header.e_phnum;

	args.insert(args.begin(), 1, start->file.path.str);
	p.init(args);

	uintptr_t entry = start->header.entry();
	LOG_INFO << "Start at " << (void*)start->base << " + " << (void*)(entry);
	p.start(start->base + entry);

	return true;
}

std::optional<VersionedSymbol> Loader::resolve_symbol(const VersionedSymbol & sym, DL::Lmid_t ns) const {
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
	return std::nullopt;
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
