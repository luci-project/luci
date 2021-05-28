#pragma once

#include "libc/assert.hpp"
#include "libc/string.hpp"
#include "libc/unistd.hpp"
#include "elf.hpp"

#include "page.hpp"

struct Object;

// TODO: Ability to merge areas, use shared mem, manage map,
struct MemorySegment {
	/*! \brief Storage area relative to file */
	struct {
		/*! \brief Object */
		const Object & object;

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

		int fd;

		/*! \brief Mapped into memory */
		bool available;

		/*! \brief get memory address */
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
	  : source{object, segment.offset(), segment.size() },
	    target{base, segment.virt_addr(), segment.virt_size(), PROT_NONE | (segment.readable() ? PROT_READ : 0) | (segment.writeable() ? PROT_WRITE : 0) | (segment.executable() ? PROT_EXEC : 0), -1, false} {}

	/*! \brief allocate in memory */
	bool map();

	/*! \brief set protection according to flags */
	bool protect();

	/*! \brief release memory allocation */
	bool unmap();
};
