#include "memory_segment.hpp"

#include <dlh/log.hpp>

#include "object/base.hpp"
#include "loader.hpp"

bool MemorySegment::map() {
	uintptr_t mem = 0;
	const bool copy = source.object.file.loader.dynamic_update
	               || source.object.data.fd < 0
	               || (source.size > 0 && (source.offset % Page::SIZE) != (target.address() % Page::SIZE))
	               || (target.protection & PROT_WRITE) != 0; // TODO: memfd
	if (copy || source.size == 0) {
		LOG_DEBUG << "Mapping " << target.page_size() << " Bytes at " << (void*)target.page_start() << " anonymous..." << endl;
		auto mmap = Syscall::mmap(target.page_start(), target.page_size(), target.protection, MAP_FIXED_NOREPLACE | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (mmap.failed()) {
			LOG_ERROR << "Mapping " << target.page_size() << " Bytes shared at " << (void*)target.page_start() << " failed: " << mmap.error_message() << endl;
			return false;
		} else if (mmap.value() != target.page_start()) {
			LOG_ERROR << "Requested shared mapping at " << (void*)target.page_start() << " but got " << mmap.value() << endl;
			return false;
		}
		// TODO: Remove, since mmap should aready zero the memory
		Memory::set(mmap.value(), 0, target.page_size());
	} else {
		auto page_offset = target.address() % Page::SIZE;
		assert(page_offset < Page::SIZE && source.offset >= page_offset);
		LOG_DEBUG << "Mapping " << target.page_size() << " Bytes at " << (void*)target.page_start() << " from file " << source.object.file.path << " at " << (source.offset - page_offset) << "..." << endl;
		auto mmap = Syscall::mmap(target.page_start(), target.page_size(), target.protection, MAP_FIXED_NOREPLACE | MAP_PRIVATE, source.object.data.fd, source.offset - page_offset);
		if (mmap.failed()) {
			LOG_ERROR << "Mapping " << target.page_size() << " Bytes private at " << (void*)target.page_start() << " failed: " << mmap.error_message() << endl;
			return false;
		} else if (mmap.value() != target.page_start()) {
			LOG_ERROR << "Requested private mapping at " << (void*)target.page_start() << " but got " << mmap.value() << endl;
			return false;
		}
	}

	if (copy && source.size > 0) {
		LOG_DEBUG << "Copy " << source.size << " Bytes from " << (void*)source.offset << " to "  << (void*)target.address() << endl;
		Memory::copy(target.address(), source.object.data.addr + source.offset, source.size);
	}

	return true;
}

bool MemorySegment::protect() {
	if (auto mprotect = Syscall::mprotect(target.page_start(), target.page_size(), target.protection)) {
		return true;
	} else {
		LOG_ERROR << "Protecting " << target.page_size() << " Bytes at " << (void*)target.page_start() << " failed: " << mprotect.error_message() << endl;
		return false;
	}
}

bool MemorySegment::unmap() {
	if (auto munmap = Syscall::munmap(target.page_start(), target.page_size())) {
		return true;
	} else {
		LOG_WARNING << "Unmapping " << (void*)target.page_start() << " (" << target.page_size() << " Bytes)" <<  " failed: " << munmap.error_message() << endl;
		return false;
	}
}
