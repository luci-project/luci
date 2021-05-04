#include "loader.hpp"

#include <sys/inotify.h>

#include <utility>

#include "xxhash64.h"

#include "process.hpp"
#include "object_file.hpp"
#include "object.hpp"

void * observer_kickoff(void * ptr) {
	reinterpret_cast<Loader *>(ptr)->observer();
	return nullptr;
}

Loader::Loader(const char * self, bool dynamicUpdate) : dynamic_update(dynamicUpdate) {
	errno = 0;
	if (::pthread_mutex_init(&mutex, nullptr) != 0) {
		LOG_ERROR << "Creating loader mutex failed: " << strerror(errno);
	} else if (dynamic_update) {
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
	if (::pthread_mutex_destroy(&mutex) != 0) {
		LOG_ERROR << "Destroying loader mutex failed: " << strerror(errno);
	} else if (dynamic_update) {
		if (close(inotifyfd) != 0)
			LOG_ERROR << "Closing inotify failed: " << strerror(errno);
		else
			LOG_INFO << "Destroyed observer background thread";
	}

	lookup.clear();
}

ObjectFile * Loader::library(const char * filename, const std::vector<const char *> & search, DL::Lmid_t ns) {
	ObjectFile * lib;
	for (auto & dir : search)
		if ((lib = file(filename, dir, ns)) != nullptr)
			return lib;
	return nullptr;
}

ObjectFile * Loader::library(const char * filename, const std::vector<const char *> & rpath, const std::vector<const char *> & runpath, DL::Lmid_t ns)  {
	ObjectFile * lib;
	for (const std::vector<const char *> & path : { rpath, library_path_runtime, runpath, library_path_config, library_path_default })
		if ((lib = library(filename, path, ns)) != nullptr)
			return lib;
	LOG_ERROR << "Library '" << filename << "' cannot be found";
	return nullptr;
}

ObjectFile * Loader::file(const char * filename, const char * directory, DL::Lmid_t ns) {
	auto filename_len = strlen(filename);
	auto directory_len = strlen(directory);
	char path[directory_len + filename_len + 2];
	::strncpy(path, directory, directory_len);
	path[directory_len] = '/';
	::strncpy(path + directory_len + 1, filename, filename_len + 1);
	return file(path, ns);
}

ObjectFile * Loader::file(const char * filepath, DL::Lmid_t ns) {
	// Get real path (if valid)
	errno = 0;
	char path[PATH_MAX];
	if (::realpath(filepath, path) == nullptr) {
		LOG_ERROR << "Opening file " << filepath << " failed: " << strerror(errno);
		return nullptr;
	}

	// Get / create file handle
	ObjectFile * object_file = nullptr;

	// New namespace?
	if (ns == DL::LM_ID_NEWLN) {
		ns = DL::LM_ID_BASE + 1;
		for (const auto & of : lookup)
			if (of.ns >= ns)
				ns = of.ns + 1;

	} else {
		// Check if handle already exists
		auto hash = ELF_Def::gnuhash(path);
		for (auto & o : lookup)
			if (o.hash == hash && o.ns == ns && &o.loader == this && strcmp(path, o.path) == 0)
				object_file = &o;
	}

	// new object, load
	if (object_file == nullptr) {
		LOG_DEBUG << "Loading " << path << "...";
		object_file = &lookup.emplace_back(*this, path, ns);
		if (!object_file->load()) {
			LOG_ERROR << "Unable to open " << path;
			lookup.pop_back();
			return nullptr;
		} else if (dynamic_update) {

		}
	}

	return object_file;
}

bool Loader::prepare() {
	// Relocate
	LOG_DEBUG << "Relocate...";
	for (auto & of : lookup)
		if (of.current->is_prepared)
			continue;
		else if (of.current->prepare())
			of.current->is_prepared = true;
		else
			return false;

	// Protect
	LOG_DEBUG << "Protect memory...";
	for (auto & of : lookup)
		if (of.current->is_protected)
			continue;
		else if (of.current->protect())
			of.current->is_protected = true;
		else
			return false;

	// TODO: Inherit is_initalized from previous?
	// Initialize (but not binary itself)
	LOG_DEBUG << "Initialize...";
	for (auto & of : lookup)
		if (of.current->is_initialized)
			continue;
		else if (of.current->initialize())
			of.current->is_initialized = true;
		else
			return false;

	return true;
}

bool Loader::run(const ObjectFile * object_file, std::vector<const char *> args, uintptr_t stack_pointer, size_t stack_size) {
	Object * start = object_file->current;
	assert(start->file_previous == nullptr);

	if (start->memory_map.size() == 0)
		return false;

	// Executed binary will be initialized in _start automatically
	start->is_initialized = true;

	// Prepare libraries
	if (!prepare()) {
		LOG_ERROR << "Preparation for execution of " << object_file->path << " failed...";
		start->is_initialized = false;
		return false;
	}

	// Prepare execution
	Process p(stack_pointer, stack_size);

	// TODO: Should not be hard coded...
	p.aux[Auxiliary::AT_PHDR] = start->base + start->elf.header.e_phoff;
	p.aux[Auxiliary::AT_PHNUM] = start->elf.header.e_phnum;

	args.insert(args.begin(), 1, start->file.path);
	p.init(args);

	uintptr_t entry = start->elf.header.entry();
	LOG_INFO << "Start at " << (void*)start->base << " + " << (void*)(entry);
	p.start(start->base + entry);

	return true;
}

std::optional<Symbol> Loader::resolve_symbol(const Symbol & sym, DL::Lmid_t ns) const {
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
					object_file.load();
					prepare();
					break;
				}
			unlock();
		}
	}
	LOG_INFO << "File Observer background thread ends.";
}
