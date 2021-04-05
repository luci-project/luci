#pragma once

#include <stdint.h>
#include <sys/mman.h>

#include "elf.hpp"

#include "page.hpp"

class Object;

struct MemorySegment {
	/*! \brief Storage area relative to file */
	struct {
		/*! \brief Object */
		const Object & object;

		/*! \brief Pointer to data */
		const void * data;

		/*! \brief start offset in file */
		const uintptr_t offset;

		/*! \brief length */
		const size_t size;
	} source;

	struct {
		/*! \brief Begin offset of virtual memory area (for dynamic objects) */
		uintptr_t base;

		/*! \brief start offset */
		uintptr_t offset;

		/*! \brief length */
		size_t size;

		/*! \brief mmap memory protection flags */
		int protection;

		uintptr_t address() const {
			return base + offset;
		}

		/*! \brief page aligned start */
		uintptr_t page_start() const {
			return address() - (address() % Page::SIZE);
		}

		/*! \brief page aligned length */
		size_t page_size() const {
			size_t tmp = ((address() - page_start()) + size + Page::SIZE - 1);
			return tmp - (tmp % Page::SIZE);
		}

		/*! \brief page aligned end */
		uintptr_t page_end() const {
			return page_start() + page_size();
		}
	} target;

	MemorySegment(const Object & object, const Elf::Segment & segment, uintptr_t base = 0)
	  : source{object, segment.data(), segment.offset(), segment.size() },
	   target{base, segment.virt_addr(), segment.virt_size(), PROT_NONE | (segment.readable() ? PROT_READ : 0) | (segment.writeable() ? PROT_WRITE : 0) | (segment.executable() ? PROT_EXEC : 0)} {}

	/*! \brief allocate in memory */
	bool map();

	/*! \brief set protection according to flags */
	bool protect();

	/*! \brief release memory allocation */
	bool unmap();
};
