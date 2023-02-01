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
	         || writable
	         || target.relro;

	int flags =  MAP_FIXED_NOREPLACE;
	int fd = -1;
	int offset = 0;
	int protection = target.protection | (writable || target.relro ? PROT_WRITE : 0);

	if (writable && source.object.use_data_alias()) {
		// Shared memory for updatable writable sections (if set, it is already initialized)
		if ((copy = (target.fd == -1))) {
			// Only first version
			assert(source.object.file_previous == nullptr);
			// Create new shared memory
			if ((target.fd = shmemfd()) == -1)
				return false;
			// This will zero the contents
			if (auto ftruncate = Syscall::ftruncate(target.fd, target.page_size())) {
				// TODO madvise(MADV_DONTFORK)
				// Seal
				if (auto fcntl = Syscall::fcntl(target.fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL); fcntl.failed()) {
					LOG_WARNING << "Sealing shared memory at fd " << target.fd << " failed: " << fcntl.error_message() << endl;
				}
				LOG_DEBUG << "Created shared memory at fd " << target.fd << endl;
			} else {
				LOG_ERROR << "Setting shared memory size failed: " << ftruncate.error_message() << endl;
				return false;
			}
		}
		flags |= MAP_SHARED;
		fd = target.fd;
	} else if (copy || source.size == 0) {
		// Anonymous mapping if not aliased and writable, emtpy or not aligned
		flags |= MAP_ANONYMOUS | MAP_PRIVATE;
	} else {
		// File backed mapping
		fd = source.object.data.fd;
		flags |= MAP_PRIVATE;
		auto page_offset = target.address() % Page::SIZE;
		assert(page_offset < Page::SIZE && source.offset >= page_offset);
		offset = source.offset - page_offset;
	}

	LOG_DEBUG << "Mapping " << target.page_size() << " Bytes (fd " << fd << ") at " << (void*)target.page_start() << "..." << endl;
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

	target.status = MEMSEG_MAPPED;
	return true;
}

bool MemorySegment::protect() {
	if (target.status == MEMSEG_NOT_MAPPED) {
		LOG_WARNING << "Cannot protect " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) of " << source.object << " since it is not mapped!" << endl;
		return false;
	} else if (target.status == MEMSEG_INACTIVE || target.status == MEMSEG_REACTIVATED) {
		LOG_DEBUG << "Inactive memory " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) of " << source.object << " should be protected -- silently skipping!" << endl;
		return true;
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

bool MemorySegment::unprotect() {
	if (target.status == MEMSEG_NOT_MAPPED) {
		LOG_WARNING << "Cannot unprotect " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) since it is not mapped!" << endl;
		return false;
	} else if (target.status == MEMSEG_INACTIVE || target.status == MEMSEG_REACTIVATED) {
		LOG_DEBUG << "Inactive memory " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) of " << source.object << " should be unprotected -- silently skipping!" << endl;
		return true;
	} else if ((target.effective_protection & PROT_WRITE) != 0) {
		return true;
	} else if (auto mprotect = Syscall::mprotect(target.page_start(), target.page_size(), target.effective_protection |PROT_WRITE )) {
		target.effective_protection |= PROT_WRITE;
		return true;
	} else {
		LOG_ERROR << "Unprotecting " << target.page_size() << " Bytes at " << (void*)target.page_start() << " failed: " << mprotect.error_message() << endl;
		return false;
	}
}

bool MemorySegment::disable() {
	switch (target.status) {
		case MEMSEG_INACTIVE:
			LOG_DEBUG << "Memory segment " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) already inactive!" << endl;
			break;
		case MEMSEG_MAPPED:
		case MEMSEG_REACTIVATED:
		{
			if ((target.protection & ~(PROT_NONE | PROT_READ | PROT_EXEC)) != 0) {
				LOG_WARNING << "Memory at " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) has potential unsuitable permissions for disabling!" << endl;
			}

			auto & identity = source.object.file;
			if (identity.loader.config.detect_outdated == Loader::Config::DETECT_OUTDATED_VIA_USERFAULTFD) {
				// TODO: This code is racy

				// create private anonymous mapping
				if (auto mmap = Syscall::mmap(target.page_start(), target.page_size(), target.protection, MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)) {
					target.status = MEMSEG_INACTIVE;
					target.effective_protection = target.protection;
				} else {
					LOG_ERROR << "Mapping (inactive) " << target.page_size() << " Bytes at " << (void*)target.page_start() << " failed: " << mmap.error_message() << endl;
					return false;
				}

				// register Userfault
				uffdio_register reg(target.page_start(), target.page_size(), UFFDIO_REGISTER_MODE_MISSING);
				if (auto ioctl = Syscall::ioctl(identity.loader.userfaultfd, UFFDIO_REGISTER, &reg)) {
					LOG_DEBUG << "Memory segment " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) disabled!" << endl;
					return true;
				} else {
					LOG_ERROR << "Registering " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) for userfault failed!" << endl;
					return false;
				}
			}
			break;
		}
		default:
			LOG_WARNING << "Cannot disable " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) since it is not mapped!" << endl;
			return false;
	}
	return true;
}

bool MemorySegment::unmap() {
	if (target.status == MEMSEG_NOT_MAPPED) {
		LOG_WARNING << "Cannot unmap " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) since it is not mapped!" << endl;
		return false;
	} else {
		auto & identity = source.object.file;
		if (target.status != MEMSEG_MAPPED && identity.loader.config.detect_outdated == Loader::Config::DETECT_OUTDATED_VIA_USERFAULTFD) {
			uffdio_range range(target.page_start(), target.page_size());
			Syscall::ioctl(identity.loader.userfaultfd, UFFDIO_UNREGISTER, &range);
		}
		if (auto munmap = Syscall::munmap(target.page_start(), target.page_size())) {
			target.status = MEMSEG_NOT_MAPPED;
			target.effective_protection = PROT_NONE;
			return true;
		} else {
			LOG_WARNING << "Unmapping " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) failed: " << munmap.error_message() << endl;
			return false;
		}
	}
}

int MemorySegment::shmemdup() {
	if (target.status != MEMSEG_MAPPED) {
		LOG_WARNING << "Cannot duplicate shared memory when it is not mapped!" << endl;
	} else {
		if (target.fd == -1)
			LOG_WARNING << "Source is not a shared memory!" << endl;
		int tmpfd = shmemfd();
		if (tmpfd != -1) {
			assert(target.status == MEMSEG_MAPPED && target.fd != -1);
			LOG_DEBUG << "Created memcopy at fd " << tmpfd << endl;

			// Try fast copy
			off_t off_target = 0;
			off_t off_tmp = 0;
			ssize_t len = target.page_size();
			while (true) {
				if (auto cfr = Syscall::copy_file_range(target.fd, &off_target, tmpfd, &off_tmp, len)) {
					if ((len -= cfr.value()) <= 0) {
						if (auto fcntl = Syscall::fcntl(tmpfd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL); fcntl.failed()) {
							LOG_WARNING << "Sealing shared memory failed: " << fcntl.error_message() << endl;
						}
						return tmpfd;
					}
				} else {
					LOG_WARNING << "Copying file range of shared memory failed: " << cfr.error_message() << " -- switching to memcpy"<< endl;
					break;
				}
			}

			// This will zero the contents
			if (auto ftruncate = Syscall::ftruncate(tmpfd, target.page_size())) {
				// Seal
				if (auto fcntl = Syscall::fcntl(tmpfd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL); fcntl.failed()) {
					LOG_WARNING << "Sealing shared memory failed: " << fcntl.error_message() << endl;
				}
				// Map
				if (auto mmap = Syscall::mmap(NULL, target.page_size(), PROT_WRITE, MAP_SHARED, tmpfd, 0)) {
					// Copy
					Memory::copy(mmap.value(), target.page_start(), target.page_size());
					// Unmap
					if (auto munmap = Syscall::munmap(mmap.value(), target.page_size()); munmap.failed())
						LOG_WARNING << "Unmapping " << (void*)mmap.value() << " (" << target.page_size() << " Bytes) failed: " << munmap.error_message() << endl;
					return tmpfd;
				} else {
					LOG_ERROR << "Mapping of memfd failed: " << mmap.error_message() << endl;
				}
			} else {
				LOG_ERROR << "Setting memfd size failed: " << ftruncate.error_message() << endl;
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
		return memfd.value();
	} else {
		LOG_ERROR << "Creating memory file " << shdata << " failed: " << memfd.error_message() << endl;
		return -1;
	}
}


extern bool capstone_dump(BufferStream & out, void * ptr, size_t size, uintptr_t start);
void MemorySegment::dump(Log::Level level) const {
	if (!logger.visible(level))
		return;

	logger.entry(level, __BASE_FILE__, __LINE__, __MODULE_NAME__)
	   << "Dump of " << source.object << " (@ " << (void*) source.offset << ", " << source.size << "B)" << endl
	   << (target.status == MEMSEG_MAPPED ? "currently" : "to be") << " mapped at " << (void*)(target.address()) << " (" << target.size << "B "
	   << ((target.protection & PROT_READ) != 0 ? "R": "")
	   << ((target.protection & PROT_WRITE) != 0 ? "W": "")
	   << ((target.protection & PROT_EXEC) != 0 ? "X": "")
	   << ")";

	void * data;
	size_t size;
	uintptr_t offset;
	if (target.status == MEMSEG_MAPPED) {
		data = reinterpret_cast<void*>(target.address());
		size = target.size;
		offset = target.offset;
	} else {
		data = reinterpret_cast<void*>(source.object.data.addr);
		size = source.size;
		offset = source.offset;
	}

	if ((target.protection & PROT_EXEC) != 0) {
		capstone_dump(logger.append() << endl, data, size, target.offset);
	} else {
		char buf[17] = {};
		for (size_t i = 0;; i++) {
			if (i % 16 == 0) {
				logger.append() << "  " << buf << endl;
				if (i >= size)
					break;
				logger.append().format("%10llx: ", offset + i);
			}
			if (i < size) {
				uint8_t byte = reinterpret_cast<uint8_t*>(data)[i];
				logger.append().format("%02x ", byte);
				buf[i % 16] = byte >= 32 && byte < 127 ? byte : '.';
			} else {
				logger.append().format("   ");
				buf[i % 16] = ' ';
			}
		}
	}
}
