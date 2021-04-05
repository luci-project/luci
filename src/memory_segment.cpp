#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>

#include "memory_segment.hpp"
#include "object.hpp"
#include "generic.hpp"

bool MemorySegment::map() {
	void * mem = nullptr;
	const bool copy = source.object.fd < 0 || (source.size > 0 && (source.offset % Page::SIZE) != (target.address() % Page::SIZE));
	if (copy || source.size == 0) {
		LOG_DEBUG << "Mapping " << target.page_size() << " Bytes at " << (void*)target.page_start() << "...";
		errno = 0;
		mem = ::mmap(reinterpret_cast<void*>(target.page_start()), target.page_size(), PROT_READ | PROT_WRITE, MAP_FIXED_NOREPLACE | MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		// TODO: Remove, since mmap should aready zero the memory
		if (mem != MAP_FAILED)
			::memset(mem, 0, target.page_size());
	} else {
		auto page_offset = target.offset - target.page_start();
		assert(page_offset >= 0 && page_offset < Page::SIZE && source.offset >= page_offset);
		errno = 0;
		mem = ::mmap(reinterpret_cast<void*>(target.page_start()), target.page_size(), PROT_READ | PROT_WRITE, MAP_FIXED_NOREPLACE | MAP_PRIVATE, source.object.fd, source.offset - page_offset);
	}
	if (mem == MAP_FAILED) {
		LOG_ERROR << "Mapping " << target.page_size() << " Bytes at " << (void*)target.page_start() << " failed: " << strerror(errno);
		return false;
	} else if (mem != (void*)target.page_start()) {
		LOG_ERROR << "Requested Mapping at " << (void*)target.page_start() << " but got " << mem;
		return false;
	}

	if (copy && source.size > 0) {
		LOG_DEBUG << "Copy " << source.size << " Bytes from " << (void*)source.offset << " to "  << (void*)target.address();
		::memcpy(reinterpret_cast<void*>(target.address()), source.data, source.size);
	}

	return true;
}

bool MemorySegment::protect() {
	errno = 0;
	if (::mprotect(reinterpret_cast<void*>(target.page_start()), target.page_size(), target.protection) == 0) {
		return true;
	} else {
		LOG_ERROR << "Protecting " << target.page_size() << " Bytes at " << (void*)target.page_start() << " failed: " << strerror(errno);
		return false;
	}
}

bool MemorySegment::unmap() {
	errno = 0;
	if (::munmap(reinterpret_cast<void*>(target.page_start()), target.page_size()) == 0) {
		return true;
	} else {
		LOG_WARNING << "Unmapping " << (void*)target.page_start() << " (" << target.page_size() << " Bytes)" <<  " failed: " << strerror(errno);
		return false;
	}
}
