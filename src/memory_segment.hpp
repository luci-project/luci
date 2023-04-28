// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <dlh/assert.hpp>
#include <dlh/string.hpp>
#include <dlh/syscall.hpp>
#include <dlh/page.hpp>
#include <dlh/log.hpp>

#include <elfo/elf.hpp>



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

		/* Mapping flags */
		int flags;

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

	uintptr_t buffer = 0;

	/*! \brief Constructor for Segments */
	MemorySegment(const Object & object, const Elf::Segment & segment, uintptr_t base = 0, uintptr_t offset_delta = 0);

	/*! \brief Constructor for Sections */
	MemorySegment(const Object & object, const Elf::Section & section, size_t target_offset, size_t target_size_delta = 0, uintptr_t base = 0);

	/*! \brief Constructor for pure BSS */
	MemorySegment(const Object & object, size_t target_offset, size_t target_size, uintptr_t base = 0);

	/*! \brief Destructor (clean up) */
	~MemorySegment();

	/*! \brief Allocate/get address of temporary (back) buffer to modify contents offline */
	uintptr_t compose();

	/*! \brief Helper to get pointer in compose buffer corresponding to an active address */
	template<typename T>
	T * compose_pointer(T * pointer) {
		return reinterpret_cast<T *>(target.contains(pointer) ? buffer + target.offset + reinterpret_cast<uintptr_t>(pointer) - target.address() : nullptr);
	}

	/*! \brief Use compositing buffer (if set) and adjust protection  */
	bool finalize(bool force = false);

	/*! \brief allocate in memory */
	bool map();

	/* \brief set (non writeable) memory inactive */
	bool disable();

	/* \brief reactivate (non writeable) memory */
	bool enable();

	/*! \brief release memory allocation */
	bool unmap();

	/*! \brief duplicate memory fd for this segment */
	int shmemdup();

	/*! \brief dump memory to log */
	void dump(Log::Level level = Log::DEBUG) const;

 private:
	/*! \brief create shared memory fd for this segment */
	int shmemfd();
};
