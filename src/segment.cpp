#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "segment.hpp"
#include "generic.hpp"

bool Segment::map(int fd, bool copy) {
	void * mem = nullptr;
	if (copy) {
		LOG_DEBUG << "Mapping " << memory.get_length() << " Bytes at " << (void*)memory.get_start() << "...";
		errno = 0;
		mem = ::mmap(reinterpret_cast<void*>(memory.get_start()), memory.get_length(), PROT_READ | PROT_WRITE, MAP_FIXED_NOREPLACE | MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		::memset(reinterpret_cast<void*>(memory.get_start()), 0, memory.get_length());
	} else {
		// TODO: Would be nice (and faster) to use COW for files. However, mmap(2) MAP_PRIVATE states:
		// ""[...] It is unspecified whether changes made to the file after the mmap() call are visible in the mapped region."
		// And our concept currently depends on file changes of the binaries...
		// Solution: use symlinks instead of replace with file version (& patch level) suffix?
		assert(false);
		//base = mmap(reinterpret_cast<void*>(memory.get_start()), memory.get_length(), protection, MAP_FIXED_NOREPLACE | MAP_PRIVATE, fd, ...);
	}
	if (mem == MAP_FAILED) {
		LOG_ERROR << "Mapping " << memory.get_length() << " Bytes at " << (void*)memory.get_start() << " failed: " << strerror(errno);
		return false;
	} else if (mem != (void*)memory.get_start()) {
		LOG_ERROR << "Requested Mapping at " << (void*)memory.get_start() << " but got " << mem;
		return false;
	}

	if (copy && file.size > 0) {
		LOG_DEBUG << "Copy " << file.size << " Bytes from file offset " << (void*)file.offset << " to "  << (void*)(memory.base) << " + " << (void*)memory.offset;
		// Set offset
		errno = 0;
		off_t off = lseek(fd, file.offset, SEEK_SET);
		if (off < 0) {
			LOG_ERROR << "Setting file offset to " << file.offset << " failed: " << strerror(errno);
			return false;
		} else if (file.offset - off != 0) {
			LOG_ERROR << "Offset " << off << " not equal to position " << file.offset;
			return false;
		}
		// Read into memory
		errno = 0;
		LOG_DEBUG << "Reading " << file.size << " Bytes into " << (void*)(memory.base) << " + " << (void*)(memory.offset) << "...";
		ssize_t delta, pos = 0;
		while ((delta = read(fd, reinterpret_cast<void*>(memory.base + memory.offset + pos), file.size - pos)) > 0) {
			pos += delta;
		}
		if (delta < 0) {
			LOG_ERROR << "Reading " << file.size << " Bytes into " << (void*)(memory.base) << " + " << (void*)(memory.offset) << " failed: " << strerror(errno);
			return false;
		} else if (file.size - pos != 0) {
			LOG_ERROR << "Reading incomplete, got only " << pos << " Bytes (instead of " << file.size << ")...";
			return false;
		}
	}

	return true;
}

bool Segment::protect() {
	errno = 0;
	if (::mprotect(reinterpret_cast<void*>(memory.get_start()), memory.get_length(), protection) == 0) {
		return true;
	} else {
		LOG_ERROR << "Protecting " << memory.get_length() << " Bytes at " << (void*)memory.get_start() << " failed: " << strerror(errno);
		return false;
	}
}

bool Segment::unmap() {
	errno = 0;
	if (::munmap((void*)memory.get_start(), memory.get_length()) == 0) {
		return true;
	} else {
		LOG_WARNING << "Unmapping " << (void*)memory.get_start() << " (" << memory.get_length() << " B)" <<  " failed: " << strerror(errno);
		return false;
	}
}
