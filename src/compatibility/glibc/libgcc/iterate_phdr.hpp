#pragma once

#include <dlh/types.hpp>

struct dl_phdr_info {
	// Base address of object
	uintptr_t dlpi_addr;

	// (Null-terminated) name of object
	const char *dlpi_name;

	/* Pointer to array of ELF program headers for this object */
	const void *dlpi_phdr;

	// # of items in dlpi_phdr
	uint16_t dlpi_phnum;

	/* The following fields were added in glibc 2.4, after the first
	  version of this structure was available.  Check the size
	  argument passed to the dl_iterate_phdr callback to determine
	  whether or not each later member is available.  */

	// Incremented when a new object may have been added
	unsigned long long dlpi_adds;

	// Incremented when an object may have been removed
	unsigned long long dlpi_subs;

	// If there is a PT_TLS segment, its module ID as used in TLS relocations, else zero
	size_t dlpi_tls_modid;

	// The address of the calling thread's instance of this module's PT_TLS segment, if it has one and it has been allocated in the calling thread, otherwise a null pointer
	uintptr_t dlpi_tls_data;


	// Constructor
	dl_phdr_info(uintptr_t addr, const char * name, void * phdr, uint16_t phnum,
	             unsigned long long adds = 0, unsigned long long subs = 0,
	             size_t tls_modid = 0, uintptr_t tls_data = 0)
	           : dlpi_addr(addr), dlpi_name(name), dlpi_phdr(phdr), dlpi_phnum(phnum),
	             dlpi_adds(adds), dlpi_subs(subs), dlpi_tls_modid(tls_modid), dlpi_tls_data(tls_data) {}
};


extern "C" int dl_iterate_phdr(int (*callback)(dl_phdr_info *info, size_t size, void *data), void *data);
