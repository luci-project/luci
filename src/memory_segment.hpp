#pragma once

#include <dlh/assert.hpp>
#include <dlh/string.hpp>
#include <dlh/syscall.hpp>

#include <elfo/elf.hpp>

#include "page.hpp"

struct Object;

// TODO: Ability to merge areas, use shared mem, manage map,
struct MemorySegment {
	enum Status {
		MEMSEG_NOT_MAPPED,
		MEMSEG_MAPPED,
		MEMSEG_INACTIVE,
		MEMSEG_REACTIVATED
	};

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

		/*! \brief memory protection flags for runtime */
		int protection;

		/*! \brief Current memory protection flags */
		int effective_protection;

		/*! \brief Memory file descriptor for shared data */
		int fd;

		/*! \brief read-only relocation */
		bool relro;

		/*! \brief Current mapping status */
		enum Status status;

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

		/*! \brief check if this segment maps an virtual address */
		bool contains(uintptr_t ptr) const {
			return ptr >= base + offset && ptr < base + offset + size;
		}

		/*! \brief check if this segment is user writable */
		bool writable() const {
			return (protection & PROT_WRITE) != 0;
		}
	} target;

	/*! \brief Constructor */
	MemorySegment(const Object & object, const Elf::Segment & segment, uintptr_t base = 0, uintptr_t offset_delta = 0)
	  : source{object, segment.offset() + offset_delta, segment.size() - offset_delta},
	    target{base, segment.virt_addr() + offset_delta, segment.virt_size() - offset_delta, PROT_NONE | (segment.readable() ? PROT_READ : 0) | (segment.writeable() ? PROT_WRITE : 0) | (segment.executable() ? PROT_EXEC : 0), PROT_NONE, -1, segment.type() == Elf::PT_GNU_RELRO, MEMSEG_NOT_MAPPED} {
		assert(!(target.relro && segment.writeable()));
		assert(target.size >= source.size);
	}

	/*! \brief Destructor (clean up) */
	~MemorySegment();

	/*! \brief allocate in memory */
	bool map();

	/*! \brief set protection according to flags */
	bool protect();

	/*! \brief unprotect (make writable)  */
	bool unprotect();

	/* \brief set (non writeable) memory inactive */
	bool disable();

	/*! \brief release memory allocation */
	bool unmap();

	/*! \brief duplicate memory fd for this segment */
	int shmemdup();

 private:
	/*! \brief create shared memory fd for this segment */
	int shmemfd();
};
