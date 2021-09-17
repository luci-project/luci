#include "memory_segment.hpp"

#include <dlh/log.hpp>

#include "object/base.hpp"
#include "loader.hpp"

MemorySegment::~MemorySegment() {
	unmap();
}


bool MemorySegment::map() {
	uintptr_t mem = 0;
	auto & identity = source.object.file;
	const bool writable = (target.protection & PROT_WRITE) != 0;
	bool copy = source.object.data.fd < 0
	         || (source.size > 0 && (source.offset % Page::SIZE) != (target.address() % Page::SIZE))
	         || writable;

	int flags =  MAP_FIXED_NOREPLACE | MAP_PRIVATE;
	int fd = -1;
	int offset = 0;
	int protection = target.protection | (writable ? PROT_WRITE : 0);

	if (identity.loader.dynamic_update && writable) {
		// Shared memory for updatable writable sections (if set, it is already initialized)
		if ((copy = (target.fd == -1))) {
			// Create new shared memory
			if ((target.fd = shmemfd()) == -1)
				return false;
		}
		fd = target.fd;
	} else if (copy || source.size == 0) {
		// Anonymous mapping if not updatable and writable, emtpy or not aligned
		flags |= MAP_ANONYMOUS;
	} else {
		// File backed mapping
		fd = source.object.data.fd;
		auto page_offset = target.address() % Page::SIZE;
		assert(page_offset < Page::SIZE && source.offset >= page_offset);
		offset = source.offset - page_offset;
	}

	LOG_DEBUG << "Mapping " << target.page_size() << " Bytes at " << (void*)target.page_start() << "..." << endl;
	auto mmap = Syscall::mmap(target.page_start(), target.page_size(), protection, flags, fd, offset);
	if (mmap.failed()) {
		LOG_ERROR << "Mapping " << target.page_size() << " Bytes at " << (void*)target.page_start() << " failed: " << mmap.error_message() << endl;
		return false;
	} else if (mmap.value() != target.page_start()) {
		LOG_ERROR << "Requested mapping at " << (void*)target.page_start() << " but got " << mmap.value() << endl;
		return false;
	}
	target.effective_protection = protection;

	if (copy && source.size > 0) {
		LOG_DEBUG << "Copy " << source.size << " Bytes from " << (void*)source.offset << " to "  << (void*)target.address() << endl;
		Memory::copy(target.address(), source.object.data.addr + source.offset, source.size);
	}

	target.available = true;
	return true;
}

bool MemorySegment::protect() {
	if (!target.available) {
		LOG_WARNING << "Cannot protect " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) since it is not mapped!" << endl;
		return false;
	} else if (target.effective_protection == target.protection) {
		return true;
	} else if (auto mprotect = Syscall::mprotect(target.page_start(), target.page_size(), target.protection)) {
		target.effective_protection = target.protection;
		return true;
	} else {
		LOG_ERROR << "Protecting " << target.page_size() << " Bytes at " << (void*)target.page_start() << " failed: " << mprotect.error_message() << endl;
		return false;
	}
}

bool MemorySegment::unmap() {
	if (!target.available) {
		LOG_WARNING << "Cannot unmap " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) since it is not mapped!" << endl;
		return false;
	} else if (auto munmap = Syscall::munmap(target.page_start(), target.page_size())) {
		target.available = false;
		target.effective_protection = PROT_NONE;
		return true;
	} else {
		LOG_WARNING << "Unmapping " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) failed: " << munmap.error_message() << endl;
		return false;
	}
}

int MemorySegment::shmemdup() {
	if (!target.available) {
		LOG_WARNING << "Cannot duplicate shared memory when it is not mapped!" << endl;
	} else {
		if (target.fd == -1)
			LOG_WARNING << "Source is not a shared memory!" << endl;
		int tmpfd = shmemfd();
		if (tmpfd != -1) {
			assert(target.available && target.fd != -1);
			if (auto mmap = Syscall::mmap(NULL, target.page_size(), PROT_WRITE, MAP_SHARED, tmpfd, 0)) {
				Memory::copy(target.page_start(), mmap.value(), target.page_size());
				if (auto munmap = Syscall::munmap(mmap.value(), target.page_size()); munmap.failed())
					LOG_WARNING << "Unmapping " << (void*)mmap.value() << " (" << target.page_size() << " Bytes) failed: " << munmap.error_message() << endl;

				return tmpfd;
			}
			Syscall::close(tmpfd);
		}
	}
	return -1;
}


int MemorySegment::shmemfd() {
	// Create shared memory for data
	StringStream<NAME_MAX + 1> shdatastr;
	shdatastr << source.object.file.name.str;
	if (source.object.data.hash != 0)
		shdatastr << '#' << hex << source.object.data.hash;
	shdatastr << "@" << hex << target.offset;
	const char * shdata = shdatastr.str();

	if (auto memfd = Syscall::memfd_create(shdata, MFD_CLOEXEC | MFD_ALLOW_SEALING)) {
		// This will zero the contents
		if (auto ftruncate = Syscall::ftruncate(memfd.value(), target.page_size())) {
			if (auto fcntl = Syscall::fcntl(memfd.value(), F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL); fcntl.failed())
				LOG_ERROR << "Sealing shared memory of " << shdata << " failed: " << fcntl.error_message() << endl;
			LOG_DEBUG << "Created memcopy " << shdata << " at fd " << memfd.value() << endl;
			return memfd.value();
		} else {
			LOG_ERROR << "Setting memcopy size of " << shdata << " failed: " << ftruncate.error_message() << endl;
		}
	} else {
		LOG_ERROR << "Creating memory file " << shdata << " failed: " << memfd.error_message() << endl;
	}
	return -1;
}
