#include "memory_segment.hpp"

#include <dlh/log.hpp>

#include "object/base.hpp"
#include "loader.hpp"


MemorySegment::MemorySegment(const Object & object, const Elf::Segment & segment, uintptr_t base, uintptr_t offset_delta)
  : source{object, segment.offset() + offset_delta, segment.size() - offset_delta},
    target{base, segment.virt_addr() + offset_delta, segment.virt_size() - offset_delta, (segment.readable() ? PROT_READ : PROT_NONE) | (segment.writeable() ? PROT_WRITE : 0) | (segment.executable() ? PROT_EXEC : 0), PROT_NONE, -1, 0, segment.type() == Elf::PT_GNU_RELRO, object.file.flags.premapped ? MEMSEG_MAPPED : MEMSEG_NOT_MAPPED} {
	assert(!(target.relro && segment.writeable()));
	assert(target.size >= source.size);
}

MemorySegment::MemorySegment(const Object & object, const Elf::Section & section, size_t target_offset, size_t target_size_delta, uintptr_t base)
  : source{object, section.offset(), section.type() == Elf::SHT_NOBITS ? 0 : section.size() },
    target{base, target_offset, section.size() + target_size_delta, PROT_READ | PROT_WRITE | (section.writeable() ? PROT_WRITE : 0) | (section.executable() ? PROT_EXEC : 0), PROT_NONE, -1, 0, false, object.file.flags.premapped ? MEMSEG_MAPPED : MEMSEG_NOT_MAPPED } {
		assert((target.address() % section.alignment()) == 0);
	}

MemorySegment::MemorySegment(const Object & object, size_t target_offset, size_t target_size, uintptr_t base)
  : source{object, 0, 0 },
	target{base, target_offset, target_size, PROT_READ | PROT_WRITE, PROT_NONE, -1, 0, false, MEMSEG_NOT_MAPPED} {}


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
	int protection = target.protection | (copy || writable || target.relro ? PROT_WRITE : 0);

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

static const int flags_privanon = MAP_ANONYMOUS | MAP_PRIVATE;
uintptr_t MemorySegment::compose() {
	if (buffer == 0) {
		// Memory segment must be mapped
		if (target.status != MEMSEG_MAPPED) {
			if (!map())
				return 0;
		}
		// Compositing is only used for non-writable segments
		if ((target.effective_protection & PROT_WRITE) != 0) {
			return target.address();
		}
		// If process has not started yet -> we can just change permissions and use current buffer
		if (!source.object.file.loader.process_started && Syscall::mprotect(target.page_start(), target.page_size(), target.effective_protection | PROT_WRITE ).success()) {
			target.effective_protection |= PROT_WRITE;
			return target.address();
		// Create anonymous writable mapping with same size as source
		} else if (auto mmap = Syscall::mmap(target.page_start() & (~0x400000000000), target.page_size(), PROT_READ | PROT_WRITE, flags_privanon, -1, 0)) {
			// Duplicate full pages (just for debugging)
			buffer = Memory::copy(mmap.value(), target.page_start(), target.page_size());
			LOG_DEBUG << "Created compose back buffer at " << (void*)buffer << " for " << (void*)target.page_start() << " (" << target.page_size() << " Bytes)" << endl;
		} else {
			LOG_ERROR << "Mapping back buffer for " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) failed: " << mmap.error_message() << endl;
		}
	}
	return buffer + (target.offset % Page::SIZE);
}

bool MemorySegment::finalize(bool force) {
	if (!force && target.status == MEMSEG_INACTIVE) {
		LOG_WARNING << "Memory segment " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) is currently disabled, hence ignoring finalize request" << endl;
		return true;
	} else if (buffer != 0) {
		bool result = true;
		// Adjust permissions
		if (auto mprotect = Syscall::mprotect(buffer, target.page_size(), target.protection)) {
			target.effective_protection = target.protection;
		} else {
			LOG_WARNING << "Unable to adjust protection for " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) of " << source.object << ": " << mprotect.error_message() << endl;
			result = false;
		}

		// Remap
		if (auto mremap = Syscall::mremap(buffer, target.page_size(), target.page_size(), MREMAP_MAYMOVE | MREMAP_FIXED, target.page_start())) {
			target.flags = flags_privanon;
			buffer = 0;
			LOG_INFO << "Updated " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) of " << source.object << " with composite back buffer (private mapping)" << endl;
		} else {
			LOG_ERROR << "Protecting " << target.page_size() << " Bytes at " << (void*)target.page_start() << " failed: " << mremap.error_message() << endl;
			result = false;
		}
		return result;
	} else if (target.status == MEMSEG_NOT_MAPPED) {
		LOG_WARNING << "Cannot protect " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) of " << source.object << " since it is not mapped!" << endl;
		return false;
	} else if (!force && target.effective_protection == target.protection) {
		return true;
	} else if (auto mprotect = Syscall::mprotect(target.page_start(), target.page_size(), target.protection)) {
		target.effective_protection = target.protection;
		return true;;
	} else {
		LOG_ERROR << "Protecting " << target.page_size() << " Bytes at " << (void*)target.page_start() << " failed: " << mprotect.error_message() << endl;
		return false;
	}
}

bool MemorySegment::disable() {
	if (target.status == MEMSEG_NOT_MAPPED) {
		LOG_WARNING << "Cannot disable " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) since it is not mapped!" << endl;
		return false;
	} else if (target.status == MEMSEG_INACTIVE) {
		LOG_DEBUG << "Memory segment " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) already inactive!" << endl;
		return true;
	} else {
		if ((target.protection & ~(PROT_NONE | PROT_READ | PROT_EXEC)) != 0) {
			LOG_WARNING << "Memory at " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) has potential unsuitable permissions for disabling!" << endl;
		}

		auto & identity = source.object.file;
		if (identity.loader.config.detect_outdated == Loader::Config::DETECT_OUTDATED_VIA_USERFAULTFD) {
			/* The only way to register a page range in userfault without having the code unmapped for few cycles
			 * is by using a a private anonymous memory with copied contents.
			 * The approach is
			 *  - create private anonymous copy of page range (if not already done yet)
			 *  - use it to replace the active target memory range (using remap)
			 *  - register range for userfault
			 *  - remap it again, but do not unmap target memory range -> userfault on access
			 * To re-enable it, the last step will be reversed.
			 */
			uintptr_t page_start = 0;
			if ((target.flags & flags_privanon) != flags_privanon) {
				// save old memory address
				page_start = buffer;
				// Create & set private mapping
				if (compose() == 0 || !finalize())
					return false;
			} else if (auto mmap = Syscall::mmap(0, target.page_size(), PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0)) {
				page_start = mmap.value();
			} else {
				// Unable to find a unused memory range
				LOG_ERROR << "Unable to map a range for " << target.page_size() << " Bytes (for userfault buffer): " << mmap.error_message() << endl;
				return false;
			}

			// Register userfaultfd
			uffdio_register reg(target.page_start(), target.page_size(), UFFDIO_REGISTER_MODE_MISSING);
			if (auto ioctl = Syscall::ioctl(identity.loader.userfaultfd, UFFDIO_REGISTER, &reg)) {
				LOG_DEBUG << "Registered Userfault for memory segment " << (void*)target.page_start() << " (" << target.page_size() << " Bytes)!" << endl;
			} else {
				LOG_ERROR << "Registering " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) for userfault failed!" << endl;
				return false;
			}

			// Move mapping again, but do not unmap old code
			if (auto mremap = Syscall::mremap(target.page_start(), target.page_size(), target.page_size(), MREMAP_MAYMOVE | MREMAP_FIXED | MREMAP_DONTUNMAP, page_start)) {
				buffer = mremap.value();
				target.status = MEMSEG_INACTIVE;
				target.effective_protection = target.protection;
				LOG_DEBUG << "Memory segment " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) disabled, contents moved to " << (void*)(mremap.value()) << endl;

				// Compose buffer shall be writeable (and not executable);
				if (auto mprotect = Syscall::mprotect(buffer, target.page_size(), PROT_READ | PROT_WRITE); mprotect.failed()) {
					LOG_ERROR << "Making compose back buffer " << (void*)buffer << " (" << target.page_size() << " Bytes) writeable failed: " << mprotect.error_message() << endl;
				}
			} else {
				LOG_ERROR << "Remapping " << target.page_size() << " Bytes at " << (void*)target.page_start() << " failed: " << mremap.error_message() << endl;
				return false;
			}
		}
		return true;
	}
}

bool MemorySegment::enable() {
	if (target.status != MEMSEG_INACTIVE) {
		LOG_WARNING << "Memory segment " << (void*)target.page_start() << " (" << target.page_size() << " Bytes) not inactive" << endl;
		return false;
	} else if (buffer == 0) {
		LOG_ERROR << "No back buffer for memory segment " << (void*)target.page_start() << " (" << target.page_size() << " Bytes)" << endl;
		return false;
	} else if (!finalize(true)) {
		LOG_WARNING << "Cannot enable memory segment " << (void*)target.page_start() << " (" << target.page_size() << " Bytes)" << endl;
		return false;
	} else {
		LOG_DEBUG << "Manually enabled memory segment " << (void*)target.page_start() << " (" << target.page_size() << " Bytes)" << endl;
		return true;
	}
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
			if (buffer != 0) {
				Syscall::munmap(buffer, target.page_size());
				buffer = 0;
			}
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
		capstone_dump(logger.append() << endl, data, size, target.address());
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
