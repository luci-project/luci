#pragma once

#include <stdint.h>
#include <sys/mman.h>

#include "page.hpp"

struct Segment {
	/*! \brief Storage area relative to file */
	struct {
		/*! \brief start offset */
		uintptr_t offset;

		/*! \brief length */
		size_t size;
	} file;


	/*! \brief Storage area in memory */
	struct {
		/*! \brief Begin of storage area (for virtual memory) */
		uintptr_t base;

		/*! \brief start offset */
		uintptr_t offset;

		/*! \brief length */
		size_t size;

		/*! \brief page aligned start */
		uintptr_t get_start() const {
			uintptr_t addr = base + offset;
			return addr - (addr % Page::SIZE);
		}

		/*! \brief page aligned length */
		size_t get_length() const {
			uintptr_t addr = base + offset;
			size_t tmp = ((addr - get_start()) + size + Page::SIZE - 1);
			return tmp - (tmp % Page::SIZE);
		}

		/*! \brief page aligned end*/
		uintptr_t get_end() const {
			return get_start() + get_length();
		}
	} memory;

	/*! \brief Memory Protection */
	int protection;

	/*! \brief allocate in memory */
	bool map(int fd, bool copy = true);

	/*! \brief set protection according to flags */
	bool protect();


	/*! \brief release memory allocation */
	bool unmap();
};
