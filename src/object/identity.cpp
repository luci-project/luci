#include "object/identity.hpp"

#include <dlh/log.hpp>
#include <dlh/file.hpp>
#include <dlh/string.hpp>
#include <dlh/xxhash.hpp>
#include <dlh/utility.hpp>
#include <dlh/syscall.hpp>

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

Object * ObjectIdentity::load(uintptr_t addr, bool preload, bool map, Elf::ehdr_type type) {
	// Not updatable: Allow only one (current) version
	if (!flags.updatable && current != nullptr)
		return nullptr;

	Object::Data data;
	LOG_DEBUG << "Loading " << name << "..." << endl;

	if (addr == 0) {
		assert(!path.empty());

		// Open file
		if (auto open = Syscall::open(path.str, O_RDONLY)) {
			data.fd = open.value();
		} else {
			LOG_VERBOSE << "Opening " << *this << " failed: " << open.error_message() << endl;
			return nullptr;
		}

		// Determine file size and inode
		if (struct stat sb; auto fstat = Syscall::fstat(data.fd, &sb)) {
			data.modification_time = sb.st_mtim;
			data.size = sb.st_size;
		} else {
			LOG_ERROR << "Stat file " << *this << " failed: " << fstat.error_message() << endl;
			Syscall::close(data.fd);
			return nullptr;
		}


		// Check if already loaded (using modification time)
		if (!flags.ignore_mtime)
			for (Object * obj = current; obj != nullptr; obj = obj->file_previous)
				if (obj->data.modification_time.tv_sec == data.modification_time.tv_sec && obj->data.modification_time.tv_nsec == data.modification_time.tv_nsec && obj->data.size == data.size) {
					LOG_INFO << "Already loaded " << *this << " with same modification time -- abort loading..." << endl;
					Syscall::close(data.fd);
					return nullptr;
				}

		// Map file
		if (auto mmap = Syscall::mmap(NULL, data.size, PROT_READ, MAP_PRIVATE, data.fd, 0)) {
			data.addr = mmap.value();
		} else {
			LOG_ERROR << "Mapping " << *this << " failed: " << mmap.error_message() << endl;
			Syscall::close(data.fd);
			return nullptr;
		}

	} else {
		data.addr = addr;
		data.size = Elf(addr).size(true);
	}

	// Check ELF
	const Elf::Header * header = reinterpret_cast<const Elf::Header *>(data.addr);
	if (data.size < sizeof(Elf::Header) || !supported(header)) {
		LOG_ERROR << "Unsupported ELF header in " << *this << "!" << endl;
		Syscall::munmap(data.addr, data.size);
		Syscall::close(data.fd);
		return nullptr;
	} else if (type == Elf::ET_NONE) {
		type = header->type();
	}

	// Create object
	Object * o = create(data, preload, map, type);
	if (o == nullptr && data.fd != -1) {
		Syscall::munmap(data.addr, data.size);
		Syscall::close(data.fd);
	}
	return o;
}

int ObjectIdentity::memfd_create(const char * suffix, uint64_t id, int flags) const {
	// Create shared memory for data
	StringStream<NAME_MAX + 1> shdatastr;
	shdatastr << name.str;
	if (suffix != nullptr)
		shdatastr << '.' << suffix;
	if (id != 0)
		shdatastr << '-' << id;
	const char * shdata = shdatastr.str();

	auto memfd_create = Syscall::memfd_create(shdata, flags);
	if (memfd_create.success()) {
		return memfd_create.value();
	} else {
		LOG_ERROR << "Creating memory file " << shdata << " failed: " << memfd_create.error_message() << endl;
		return -1;
	}
}

ObjectIdentity::ObjectIdentity(Loader & loader, const char * path, namespace_t ns, const char * altname) : ns(ns), loader(loader), name(altname) {
	assert(ns != NAMESPACE_NEW);

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
		auto pathlen = String::len(path) + 1;
		char tmp[pathlen];
		String::copy(tmp, path, pathlen);
		auto tmpfilename = String::find_last(tmp, '/');
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
			String::copy(buffer + bufferlen, tmpfilename, PATH_MAX - bufferlen);
		} else {
			String::copy(buffer, path, PATH_MAX);
		}
		this->path = StrPtr(buffer);
		if (altname == nullptr)
			this->name = this->path.find_last('/');

		// Observe file?
		if (flags.updatable) {
			struct stat sb;
			auto lstat = Syscall::lstat(this->path.str, &sb);
			if (lstat.failed()) {
				LOG_ERROR << "Lstat of " << path << " failed: " << lstat.error_message() << endl;
			} else if (S_ISLNK(sb.st_mode)) {
				LOG_DEBUG << "Library " << this->path << " is a symbolic link (hence we do not expect changes to the binary itself)" << endl;
				flags.immutable_source = 1;
			}

			if (auto inotify = Syscall::inotify_add_watch(loader.inotifyfd, this->path.str, IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF | IN_DONT_FOLLOW)) {
				LOG_DEBUG << "Watching for modifications at " << this->path << endl;
				wd = inotify.value();
			} else {
				LOG_ERROR << "Cannot watch for modification of " << this->path << ": " << inotify.error_message() << endl;
			}
		}
	}
	// GLIBC related
	filename = buffer;
}

ObjectIdentity::~ObjectIdentity() {
	if (loader.dynamic_update) {
		if (auto inotify = Syscall::inotify_rm_watch(loader.inotifyfd, wd); inotify.failed())
			LOG_ERROR << "Removing watch for " << this->path << " failed: " << inotify.error_message() << endl;
	}
	// Delete all versions
	while (current != nullptr)
		delete current;
}


Object * ObjectIdentity::create(Object::Data & data, bool preload, bool map, Elf::ehdr_type type) {
	// Hash file contents
	if (flags.updatable) {
		XXHash64 datahash(name.hash);  // Name hash as seed
		datahash.add(data.addr, data.size);
		data.hash = datahash.hash();
		LOG_DEBUG << "Elf " << *this << " has hash " << hex << data.hash << dec << endl;

		// Check if already loaded (using hash)
		for (Object * obj = current; obj != nullptr; obj = obj->file_previous)
			if (obj->data.hash == data.hash && obj->data.size == data.size) {
				LOG_INFO << "Already loaded " << name << " with same hash -- abort loading..." << endl;
				return nullptr;
			}
	}

	/* TODO:
	 * 1. Load file
	 * 2. hash & bean (if required)
	 * 3. Map (load or copy) to target memory if EXEC or DYN && current = 0
	 * 4. Create object
	 */

	/*
	// Temporary Elf object
	Elf tmp(data.addr);
	uintptr_t base = 0;
	switch (type) {
		case Elf::ET_DYN:
			base = file.loader.next_address();
			[[fallthrough]];
		case Elf::ET_EXEC:
			// load segments
			for (const auto & segment : tmp.segments)
				if (Elf::PT_LOAD == segment.type() && segment.virt_size() > 0)
					memory_map.emplace_back(tmp, segment, base);

	*/

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
			o = new ObjectDynamic{*this, data, !map};
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
		LOG_INFO << "Calculate Binary hash of " << *this << endl;
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
	if (current == o) {
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
	int tmpfd = memfd_create("COPY", data.hash, MFD_CLOEXEC | MFD_ALLOW_SEALING);
	if (tmpfd < 0)
		return false;

	if (auto ftruncate = Syscall::ftruncate(tmpfd, data.size)) {
		if (auto mmap = Syscall::mmap(NULL, data.size, PROT_READ | PROT_WRITE, MAP_SHARED, tmpfd, 0)) {
			Memory::copy(mmap.value(), data.addr, data.size);
			if (auto mprotect = Syscall::mprotect(mmap.value(), data.size, PROT_READ)) {
				if (auto fcntl = Syscall::fcntl(tmpfd, F_ADD_SEALS, F_SEAL_FUTURE_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL)) {
					LOG_INFO << "Created memory copy of " << *this << endl;
					// Clean up old memdup
					if (data.fd != -1) {
						if (auto munmap = Syscall::munmap(data.addr, data.size); munmap.failed())
							LOG_WARNING << "Unable to unmap file " << *this << ": " << munmap.error_message() << endl;

						if (auto close = Syscall::close(data.fd); close.failed())
							LOG_WARNING << "Unable to close file " << *this << ": " << close.error_message() << endl;
					}

					data.fd = tmpfd;
					data.addr = mmap.value();
					return true;
				} else {
					LOG_ERROR << "Sealing memcopy of " << *this << " (read-only) failed: " << fcntl.error_message() << endl;
				}
			} else {
				LOG_ERROR << "Protecting memcopy of " << *this << " (read-only) failed: " << mprotect.error_message() << endl;
			}

			// Clean up
			auto munmap = Syscall::munmap(mmap.value(), data.size);
			if (!munmap.success())
				LOG_WARNING << "Unable to unmap file " << *this << ": " << munmap.error_message() << endl;
		} else {
			LOG_ERROR << "Mapping memcopy of " << *this << " failed: " << mmap.error_message() << endl;
		}
	} else {
		LOG_ERROR << "Setting memcopy size of " << *this << " failed: " << ftruncate.error_message() << endl;
	}

	// Clean up
	Syscall::close(tmpfd);

	return false;
}

bool ObjectIdentity::prepare() {
	assert(current != nullptr);
	switch (current->status) {
		case Object::STATUS_MAPPED:
			current->status = Object::STATUS_PREPARING;

			// First initialize all dependencies
			for (auto & dep : current->dependencies)
				if (!dep->prepare())
					return false;

			LOG_DEBUG << "Preparing " << *this << endl;
			if (current->prepare())
				current->status = Object::STATUS_PREPARED;
			else
				return false;
			break;

		case Object::STATUS_PREPARING:
			LOG_WARNING << "Circular dependency on " << *this << endl;
			break;

		case Object::STATUS_PREPARED:
			break;
	}
	return true;
}

bool ObjectIdentity::initialize() {
	assert(current != nullptr);
	if (flags.initialized == 0) {
		flags.initialized = 1;

		// First initialize all dependencies
		for (auto & dep : current->dependencies)
			if (!dep->initialize())
				return false;

		LOG_DEBUG << "Initializing " << *this << endl;
		if (!current->initialize())
			return false;
	}
	return true;
}
