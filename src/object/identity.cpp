#include "object/identity.hpp"

#include <dlh/string.hpp>
#include <dlh/unistd.hpp>
#include <dlh/utility.hpp>
#include <dlh/utils/log.hpp>
#include <dlh/utils/file.hpp>
#include <dlh/utils/xxhash.hpp>

#include "object/base.hpp"
#include "object/dynamic.hpp"
#include "object/executable.hpp"
#include "object/relocatable.hpp"

#include "loader.hpp"

static bool supported(const Elf::Header * header) {
	if (!header->valid()) {
		LOG_ERROR << "No valid ELF identification header!" << endl;
		return false;
	}

	switch (header->ident_abi()) {
		case Elf::ELFOSABI_NONE:
		case Elf::ELFOSABI_LINUX:
			break;

		default:
			LOG_ERROR << "Unsupported OS ABI " << (int)header->ident_abi() << endl;
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
			LOG_ERROR << "Unsupported machine!" << endl;
			return false;
	}

	// supported
	return true;
}

Object * ObjectIdentity::load(void * ptr, bool preload, bool map, Elf::ehdr_type type) {
	// Not updatable: Allow only one (current) version
	if (!flags.updatable && current != nullptr)
		return nullptr;

	Object::Data data;
	LOG_DEBUG << "Loading " << name << "..." << endl;

	if (ptr == nullptr) {
		assert(!path.empty());

		// Open file
		errno = 0;
		if ((data.fd = ::open(path.str, O_RDONLY)) < 0) {
			LOG_VERBOSE << "Opening " << *this << " failed: " << strerror(errno) << endl;
			return nullptr;
		}

		// Determine file size and inode
		struct stat sb;
		if (::fstat(data.fd, &sb) == -1) {
			LOG_ERROR << "Stat file " << *this << " failed: " << strerror(errno) << endl;
			::close(data.fd);
			return nullptr;
		}
		data.modification_time = sb.st_mtim;
		data.size = sb.st_size;

		// Check if already loaded (using modification time)
		if (!flags.ignore_mtime)
			for (Object * obj = current; obj != nullptr; obj = obj->file_previous)
				if (obj->data.modification_time.tv_sec == data.modification_time.tv_sec && obj->data.modification_time.tv_nsec == data.modification_time.tv_nsec && obj->data.size == data.size) {
					LOG_INFO << "Already loaded " << *this << " with same modification time -- abort loading..." << endl;
					::close(data.fd);
					return nullptr;
				}

		// Map file
		if ((data.ptr = ::mmap(NULL, data.size, PROT_READ, MAP_SHARED, data.fd, 0)) == MAP_FAILED) {
			LOG_ERROR << "Mapping " << *this << " failed: " << strerror(errno) << endl;
			::close(data.fd);
			return nullptr;
		}

	} else {
		data.ptr = ptr;
		data.size = Elf(ptr).size(true);
	}

	// Check ELF
	const Elf::Header * header = reinterpret_cast<const Elf::Header *>(data.ptr);
	if (data.size < sizeof(Elf::Header) || !supported(header)) {
		LOG_ERROR << "Unsupported ELF header in " << *this << "!" << endl;
		return nullptr;
	} else if (type == Elf::ET_NONE) {
		type = header->type();
	}

	// Create object
	Object * o = create(data, preload, map, type);
	if (o == nullptr && data.fd != -1) {
		::munmap(data.ptr, data.size);
		::close(data.fd);
	}
	return o;
}

ObjectIdentity::ObjectIdentity(Loader & loader, const char * path, DL::Lmid_t ns, const char * altname) : ns(ns), loader(loader), name(altname) {
	assert(ns != DL::LM_ID_NEWLN);

	// Dynamic updates?
	if (loader.dynamic_update)
		flags.updatable = 1;
	else
		flags.immutable_source = 1;  // Without updates, don't expect changes to the binaries during runtime

	// File based?
	if (path == nullptr) {
		buffer[0] = '\0';
	} else {
		// We need the absolute path to the directory (GLIBC requirement...)
		auto pathlen = strlen(path) + 1;
		char tmp[pathlen];
		::strncpy(tmp, path, pathlen);
		auto tmpfilename = ::strrchr(tmp, '/');
		size_t bufferlen;
		bool success;
		if (tmpfilename == nullptr) {
			success = File::absolute(".", buffer, PATH_MAX, bufferlen);
			tmpfilename = tmp;
		} else {
			*(tmpfilename++) = '\0';
			success = File::absolute(tmp, buffer, PATH_MAX, bufferlen);
		}
		if (success) {
			if (bufferlen > 0)
				buffer[bufferlen++] = '/';
			::strncpy(buffer + bufferlen, tmpfilename, PATH_MAX - bufferlen);
		} else {
			::strncpy(buffer, path, PATH_MAX);
		}
		this->path = StrPtr(buffer);
		if (altname == nullptr)
			this->name = this->path.rchr('/');

		// Observe file?
		if (flags.updatable) {
			struct stat sb;
			errno = 0;
			if (lstat(this->path.str, &sb) == -1) {
				LOG_ERROR << "Lstat of " << path << " failed: " << strerror(errno) << endl;
			} else if (S_ISLNK(sb.st_mode)) {
				LOG_DEBUG << "Library " << this->path << " is a symbolic link (hence we do not expect changes to the binary itself)" << endl;
				flags.immutable_source = 1;
			}

			if ((wd = ::inotify_add_watch(loader.inotifyfd, this->path.str, IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF | IN_DONT_FOLLOW)) == -1) {
				LOG_ERROR << "Cannot watch for modification of " << this->path << ": " << ::strerror(errno) << endl;
			} else {
				LOG_DEBUG << "Watching for modifications at " << this->path << endl;
			}
		}
	}
	// GLIBC related
	filename = buffer;

	// Create shared memory for data
	errno = 0;
	StringStream<NAME_MAX + 1> shdatastr;
	shdatastr << name.str << ".DATA";
	const char * shdata = shdatastr.str();

	if ((memfd = memfd_create(shdata, MFD_CLOEXEC)) == -1) {
		LOG_ERROR << "Creating memory file " << shdata << " failed: " << strerror(errno) << endl;
	}
}

ObjectIdentity::~ObjectIdentity() {
	errno = 0;
	if (loader.dynamic_update && ::inotify_rm_watch(loader.inotifyfd, wd) == -1) {
		LOG_ERROR << "Removing watch for " << this->path << " failed: " << ::strerror(errno) << endl;
	}

	// Delete all versions
	while (current != nullptr) {
		delete current;
	}
}


Object * ObjectIdentity::create(Object::Data & data, bool preload, bool map, Elf::ehdr_type type) {
	// Hash file contents
	if (flags.updatable) {
		XXHash64 datahash(name.hash);  // Name hash as seed
		datahash.add(data.ptr, data.size);
		data.hash = datahash.hash();
		LOG_DEBUG << "Elf " << *this << " has hash " << hex << data.hash << dec << endl;

		// Check if already loaded (using hash)
		for (Object * obj = current; obj != nullptr; obj = obj->file_previous)
			if (obj->data.hash == data.hash && obj->data.size == data.size) {
				LOG_INFO << "Already loaded " << name << " with same hash -- abort loading..." << endl;
				return nullptr;
			}
	}

	// Copy contents into memory (if changes on the underlying file are possible)
	if (!flags.immutable_source && !memdup(data))
		return nullptr;

	// Create object
	Object * o = nullptr;
	switch (type) {
		case Elf::ET_EXEC:
			o = new ObjectExecutable{*this, data};
			break;
		case Elf::ET_DYN:
			o = new ObjectDynamic{*this, data};
			break;
		case Elf::ET_REL:
			o = new ObjectRelocatable{*this, data};
			break;
		default:
			LOG_ERROR << "Unsupported ELF type!" << endl;
			return nullptr;
	}
	if (o == nullptr) {
		LOG_ERROR << "unable to create object" << endl;
		return nullptr;
	}
	o->file_previous = current;

	// If dynamic updates are enabled, calculate function hashes
	if (flags.updatable) {
		o->binary_hash.emplace(*o);
		// if previous version exist, check if we can patch it
		if (current != nullptr) {
			assert(current->binary_hash && o->binary_hash);
			if (!o->patchable()) {
				LOG_ERROR << "Got new version of " << path << ", however, it is incompatible with previous data..." << endl;
				delete o;
				return nullptr;
			}
		}
	}
	// Add to list
	current = o;

	// perform preload and map memory

	if (preload && !o->preload()) {
		LOG_ERROR << "Loading of " << path << " failed (while preloading)..." << endl;
		current = o->file_previous;
		delete o;
		return nullptr;
	} else if (map && !o->map()) {
		LOG_ERROR << "Loading of " << path << " failed (while mapping into memory)..." << endl;
		current = o->file_previous;
		delete o;
		return nullptr;
	} else {
		LOG_INFO << "Successfully loaded " << path << " at " << (void*)o  << endl;
	}

	// Initialize GLIBC specific stuff (on first version only)
	if (base == 0) {
		base = o->base;
		for (const auto & segment : o->segments)
			if (segment.type() == Elf::PT_DYNAMIC) {
				dynamic = base + segment.virt_addr();
				break;
			}
	}

	return o;
}

bool ObjectIdentity::memdup(Object::Data & data) {
	int dupfd = -1;
	void * dupptr = nullptr;

	StringStream<NAME_MAX + 1> memnamestr;
	memnamestr << name.str << '.' << hex << data.hash;
	const char * memname = memnamestr.str();

	errno = 0;
	if ((dupfd = memfd_create(memname, MFD_CLOEXEC | MFD_ALLOW_SEALING)) == -1) {
		LOG_ERROR << "Creating memory file " << memname << " failed: " << strerror(errno) << endl;
	} else if (ftruncate(dupfd, data.size) == -1) {
		LOG_ERROR << "Setting size of " << memname << " failed: " << strerror(errno) << endl;
		::close(dupfd);
		dupfd = -1;
	} else if ((dupptr = ::mmap(NULL, data.size, PROT_READ | PROT_WRITE, MAP_SHARED, dupfd, 0)) == MAP_FAILED) {
		LOG_ERROR << "Mapping " << memname << " failed: " << strerror(errno) << endl;
		::close(dupfd);
		dupfd = -1;
	} else if (::memcpy(dupptr, data.ptr, data.size) != dupptr) {
		// this never happens
		assert(false);
	} else if (::mprotect(dupptr, data.size, PROT_READ) == -1) {
		LOG_ERROR << "Protecting " << memname << " (read-only) failed: " << strerror(errno) << endl;
		::close(dupfd);
		::munmap(dupptr, data.size);
		dupfd = -1;
		dupptr = nullptr;
	} else if (fcntl(dupfd, F_ADD_SEALS, F_SEAL_FUTURE_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) == -1) {
		LOG_ERROR << "Sealing " << memname << " (read-only) failed: " << strerror(errno) << endl;
		::close(dupfd);
		::munmap(dupptr, data.size);
		dupfd = -1;
		dupptr = nullptr;
	} else {
		LOG_INFO << "Created memory copy of " << *this << " in " << memname << endl;
	}

	if (dupptr == nullptr) {
		return false;
	} else {
		// Clean up
		if (data.fd != -1) {
			if (::munmap(data.ptr, data.size) == -1) {
				LOG_WARNING << "Unable to unmap file " << *this << ": " << strerror(errno) << endl;
			} else if (::close(data.fd) == -1) {
				LOG_WARNING << "Unable to close file " << *this << ": " << strerror(errno) << endl;
			}
		}

		data.fd = dupfd;
		data.ptr = dupptr;
		return true;
	}
}

bool ObjectIdentity::initialize() {
	assert(current != nullptr);
	if (flags.initialized == 0) {
		flags.initialized = 1;

		for (auto & dep : current->dependencies)
			if (!dep->initialize())
				return false;

		LOG_DEBUG << "Initializing " << *this << endl;
		if (!current->initialize())
			return false;
	}
	return true;
}
