#include "object_file.hpp"

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

Object * ObjectFile::load() {
	assert(loader.dynamic_update || current == nullptr);

	Object::Data data;
	LOG_DEBUG << "Opening " << path << "...";

	// Open file
	errno = 0;
	if ((data.fd = ::open(path, O_RDONLY)) < 0) {
		LOG_ERROR << "Reading file " << path << " failed: " << strerror(errno);
		return nullptr;
	}

	// Determine file size and inode
	struct stat sb;
	if (::fstat(data.fd, &sb) == -1) {
		LOG_ERROR << "Stat file " << path << " failed: " << strerror(errno);
		::close(data.fd);
		return nullptr;
	}
	data.size = sb.st_size;

	// Map file
	if ((data.ptr = ::mmap(NULL, data.size, PROT_READ, MAP_PRIVATE, data.fd, 0)) == MAP_FAILED) {
		LOG_ERROR << "Mmap file " << path << " failed: " << strerror(errno);
		::close(data.fd);
		return nullptr;
	}

	// Check ELF
	const Elf::Header * header = reinterpret_cast<const Elf::Header *>(data.ptr);
	if (data.size < sizeof(Elf::Header) || !supported(header)) {
		LOG_ERROR << "Unsupported ELF header in " << path << "!";
		return nullptr;
	}

	// Hash file contents
	if (loader.dynamic_update) {
		XXHash64 datahash(hash);  // Path hash as seed
		datahash.add(data.ptr, data.size);
		data.hash = datahash.hash();
		LOG_DEBUG << "File " << path << " has hash " << std::hex << data.hash << std::dec;
	}

	// Check if already loaded
	for (Object * obj = current; obj != nullptr; obj = obj->file_previous)
		if (obj->data.hash == data.hash && obj->data.size == data.size) {
			LOG_INFO << "Already loaded " << path << " -- abort loading...";
			::munmap(data.ptr, data.size);
			::close(data.fd);
			return obj;
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
		o->binary_hash.emplace(o->elf);
		// if previous version exist, check if we can patch it
		if (current != nullptr) {
			assert(current->binary_hash && o->binary_hash);
			if (!o->patchable()) {
				LOG_ERROR << "Got new version of " << path << ", however, it is incompatible with previous version...";
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


ObjectFile::ObjectFile(Loader & loader, const char * path, DL::Lmid_t ns)
  : loader(loader), hash(ELF_Def::gnuhash(path)), ns(ns) {
	flags.value = 0;
	assert(path != nullptr);
	::strcpy(this->path, path);
	assert(ns != DL::LM_ID_NEWLN);
	if (loader.dynamic_update) {
		errno = 0;
		if ((wd = ::inotify_add_watch(loader.inotifyfd, this->path, IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF)) == -1) {
			LOG_ERROR << "Cannot watch for modification of " << this->path << ": " << ::strerror(errno);
		} else {
			LOG_DEBUG << "Watching for modifications at " << this->path;
		}
	}
}

ObjectFile::~ObjectFile() {
	errno = 0;
	if (loader.dynamic_update && ::inotify_rm_watch(loader.inotifyfd, wd) == -1) {
		LOG_ERROR << "Removing watch for " << this->path << " failed: " << ::strerror(errno);
	}

	// Delete all versions
	while (current != nullptr) {
		delete current;
	}

	::free(const_cast<char*>(path));
}
