#include "object_identity.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <cstdlib>

#include <utility>

#include "elf.hpp"
#include "xxhash64.h"

#include "loader.hpp"
#include "object.hpp"
#include "object_dyn.hpp"
#include "object_rel.hpp"
#include "object_exec.hpp"
#include "generic.hpp"

#include "output.hpp"

static bool supported(const Elf::Header * header) {
	if (!header->valid()) {
		LOG_ERROR << "No valid ELF identification header!";
		return false;
	}

	switch (header->ident_abi()) {
		case Elf::ELFOSABI_NONE:
		case Elf::ELFOSABI_LINUX:
			break;

		default:
			LOG_ERROR << "Unsupported OS ABI " << (int)header->ident_abi();
			return false;
	}

	// Supported architecture?
	switch (header->machine()) {
#ifdef __i386__
		case Elf::EM_386:
		case Elf::EM_486:
			break;
#endif
#ifdef __x86_64__
		case Elf::EM_X86_64:
			break;
#endif
		default:
			LOG_ERROR << "Unsupported machine!" ;
			return false;
	}

	// supported
	return true;
}

Object * ObjectIdentity::load() {
	assert(loader.dynamic_update || current == nullptr);

	Object::Data data;
	LOG_DEBUG << "Opening " << name << "...";

	// Open file
	errno = 0;
	if ((data.fd = ::open(path.str, O_RDONLY)) < 0) {
		LOG_VERBOSE << "Opening " << *this << " failed: " << strerror(errno);
		return nullptr;
	} else {
		LOG_DEBUG << "Found " << *this;
	}

	// Determine file size and inode
	struct stat sb;
	if (::fstat(data.fd, &sb) == -1) {
		LOG_ERROR << "Stat file " << *this << " failed: " << strerror(errno);
		::close(data.fd);
		return nullptr;
	}
	data.modification_time = sb.st_mtim;
	data.size = sb.st_size;

	// Check if already loaded (using modification time)
	if (!flags.ignore_mtime)
		for (Object * obj = current; obj != nullptr; obj = obj->file_previous)
			if (obj->data.modification_time.tv_sec == data.modification_time.tv_sec && obj->data.modification_time.tv_nsec == data.modification_time.tv_nsec && obj->data.size == data.size) {
				LOG_INFO << "Already loaded " << *this << " with same modification time -- abort loading...";
				::close(data.fd);
				return obj;
			}

	// Map file
	if ((data.ptr = ::mmap(NULL, data.size, PROT_READ, MAP_SHARED, data.fd, 0)) == MAP_FAILED) {
		LOG_ERROR << "Mapping " << *this << " failed: " << strerror(errno);
		::close(data.fd);
		return nullptr;
	}

	// Check ELF
	const Elf::Header * header = reinterpret_cast<const Elf::Header *>(data.ptr);
	if (data.size < sizeof(Elf::Header) || !supported(header)) {
		LOG_ERROR << "Unsupported ELF header in " << *this << "!";
		return nullptr;
	}

	// Hash file contents
	if (loader.dynamic_update) {
		XXHash64 datahash(name.hash);  // Name hash as seed
		datahash.add(data.ptr, data.size);
		data.hash = datahash.hash();
		LOG_DEBUG << "File " << *this << " has hash " << std::hex << data.hash << std::dec;
	}

	// Check if already loaded (using hash)
	for (Object * obj = current; obj != nullptr; obj = obj->file_previous)
		if (obj->data.hash == data.hash && obj->data.size == data.size) {
			LOG_INFO << "Already loaded " << name << " with same hash -- abort loading...";
			::munmap(data.ptr, data.size);
			::close(data.fd);
			return obj;
		}


	// Copy contents into memory (if changes on the underlying file are possible)
	if (loader.dynamic_update && !symlinked) {
		int oldfd = data.fd;
		void * oldptr = data.ptr;

		errno = 0;
		char memname[NAME_MAX + 1];
		if (snprintf(memname, NAME_MAX + 1, "%s.%lx", name.str, data.hash) < 0) {
			LOG_ERROR << "Creating name for memory file " << name << " (hash " << data.hash << ") failed: " << strerror(errno);
			::munmap(oldptr, data.size);
			::close(oldfd);
			return nullptr;
		} else if ((data.fd = memfd_create(memname, MFD_CLOEXEC | MFD_ALLOW_SEALING)) == -1) {
			LOG_ERROR << "Creating memory file " << memname << " failed: " << strerror(errno);
			::munmap(oldptr, data.size);
			::close(oldfd);
			return nullptr;
		} else if (ftruncate(data.fd, data.size) == -1) {
			LOG_ERROR << "Setting size of " << memname << " failed: " << strerror(errno);
			::close(data.fd);
			::munmap(oldptr, data.size);
			::close(oldfd);
			return nullptr;
		} else if ((data.ptr = ::mmap(NULL, data.size, PROT_READ | PROT_WRITE, MAP_SHARED, data.fd, 0)) == MAP_FAILED) {
			LOG_ERROR << "Mapping " << memname << " failed: " << strerror(errno);
			::close(data.fd);
			::munmap(oldptr, data.size);
			::close(oldfd);
			return nullptr;
		} else if (::memcpy(data.ptr, oldptr, data.size) != data.ptr) {
			// this never happens
			assert(false);
		} else if (::mprotect(data.ptr, data.size, PROT_READ) == -1) {
			LOG_ERROR << "Protecting " << memname << " (read-only) failed: " << strerror(errno);
			::close(data.fd);
			::munmap(data.ptr, data.size);
			::close(oldfd);
			return nullptr;
		} else if (fcntl(data.fd, F_ADD_SEALS, F_SEAL_FUTURE_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) == -1) {
			LOG_ERROR << "Sealing " << memname << " (read-only) failed: " << strerror(errno);
			::close(data.fd);
			::munmap(data.ptr, data.size);
			::close(oldfd);
			return nullptr;
		} else if (::munmap(oldptr, data.size) == -1) {
			LOG_WARNING << "Unable to unmap file " << *this << ": " << strerror(errno);
		} else if (::close(oldfd) == -1) {
			LOG_WARNING << "Unable to close file " << *this << ": " << strerror(errno);
		} else {
			LOG_INFO << "Created memory copy of " << *this << " in " << memname;
		}

		header = reinterpret_cast<const Elf::Header *>(data.ptr);
	}

	// Create object
	Object * o = nullptr;
	switch (header->type()) {
		case Elf::ET_EXEC:
			LOG_DEBUG << "Executable (standalone)";
			o = new ObjectExecutable{*this, data};
			break;
		case Elf::ET_DYN:
			LOG_DEBUG << "Dynamic";
			o = new ObjectDynamic{*this, data};
			break;
		case Elf::ET_REL:
			LOG_DEBUG << "Relocatable";
			o = new ObjectRelocatable{*this, data};
			break;
		default:
			LOG_ERROR << "Unsupported ELF type!";
			return nullptr;
	}

	if (o == nullptr) {
		LOG_ERROR << "unable to create object";
		::close(data.fd);
		return nullptr;
	}
	o->file_previous = current;

	// If dynamic updates are enabled, calculate hash
	if (loader.dynamic_update) {
		o->binary_hash.emplace(*o);
		// if previous version exist, check if we can patch it
		if (current != nullptr) {
			assert(current->binary_hash && o->binary_hash);
			if (!o->patchable()) {
				LOG_ERROR << "Got new version of " << path << ", however, it is incompatible with previous data...";
				delete o;
				return nullptr;
			}
		}
	}
	// Add to list
	current = o;

	// perform preload and allocate memory
	if (!o->preload()) {
		LOG_ERROR << "Loading of " << path << " failed (while preloading)...";
	} else if (!o->map()) {
		LOG_ERROR << "Loading of " << path << " failed (while mapping into memory)...";
	} else {
		LOG_INFO << "Successfully loaded " << path << " at " << (void*)o ;
		return o;
	}
	delete o;
	return nullptr;
}


ObjectIdentity::ObjectIdentity(Loader & loader, const char * path, DL::Lmid_t ns) : loader(loader), ns(ns) {
	assert(path != nullptr);
	assert(ns != DL::LM_ID_NEWLN);

	::strncpy(buffer, path, PATH_MAX);
	this->path = StrPtr(buffer);
	this->name = this->path.rchr('/');

	if (loader.dynamic_update) {
		errno = 0;
		char shared_data[name.len + 6];
		if (snprintf(shared_data, NAME_MAX + 1, "%s.DATA", name.str) < 0) {
			LOG_ERROR << "Creating name for shared data memory file of " << name << " failed: " << strerror(errno);
		} else if ((memfd = memfd_create(shared_data, MFD_CLOEXEC)) == -1) {
			LOG_ERROR << "Creating memory file " << (const char *)(shared_data) << " failed: " << strerror(errno);
		}

		struct stat sb;
		if (lstat(this->path.str, &sb) == -1) {
			LOG_ERROR << "Lstat of " << path << " failed: " << strerror(errno);
		} else if (S_ISLNK(sb.st_mode)) {
			LOG_DEBUG << "Library " << this->path << " is a symbolic link (hence we do not expect changes to the binary itself)";
			symlinked = true;
		}

		if ((wd = ::inotify_add_watch(loader.inotifyfd, this->path.str, IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF | IN_DONT_FOLLOW)) == -1) {
			LOG_ERROR << "Cannot watch for modification of " << this->path << ": " << ::strerror(errno);
		} else {
			LOG_DEBUG << "Watching for modifications at " << this->path;
		}
	}
}

ObjectIdentity::~ObjectIdentity() {
	errno = 0;
	if (loader.dynamic_update && ::inotify_rm_watch(loader.inotifyfd, wd) == -1) {
		LOG_ERROR << "Removing watch for " << this->path << " failed: " << ::strerror(errno);
	}

	// Delete all versions
	while (current != nullptr) {
		delete current;
	}
}

bool ObjectIdentity::initialize() {
	assert(current != nullptr);
	if (flags.initialized == 0) {
		flags.initialized = 1;

		for (auto & dep : current->dependencies)
			if (!dep->initialize())
				return false;

		LOG_DEBUG << "Initializing " << *this;
		if (!current->initialize())
			return false;
	}
	return true;
}
