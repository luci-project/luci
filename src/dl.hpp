#pragma once

#include <cstddef>
#include <cstdint>

struct Object;

namespace DL {

enum : uintptr_t {
	RTLD_NEXT = static_cast<uintptr_t>(-1l),
	RTLD_DEFAULT = 0
};

enum Info {
	/* Treat ARG as `lmid_t *'; store namespace ID for HANDLE there.  */
	RTLD_DI_LMID = 1,
	/* Treat ARG as `struct link_map **';
	   store the `struct link_map *' for HANDLE there.  */
	RTLD_DI_LINKMAP = 2,
	RTLD_DI_CONFIGADDR = 3,        /* Unsupported, defined by Solaris.  */
	/* Treat ARG as `Dl_serinfo *' (see below), and fill in to describe the
	   directories that will be searched for dependencies of this object.
	   RTLD_DI_SERINFOSIZE fills in just the `dls_cnt' and `dls_size'
	   entries to indicate the size of the buffer that must be passed to
	   RTLD_DI_SERINFO to fill in the full information.  */
	RTLD_DI_SERINFO = 4,
	RTLD_DI_SERINFOSIZE = 5,
	/* Treat ARG as `char *', and store there the directory name used to
	   expand $ORIGIN in this shared object's dependency file names.  */
	RTLD_DI_ORIGIN = 6,
	RTLD_DI_PROFILENAME = 7,        /* Unsupported, defined by Solaris.  */
	RTLD_DI_PROFILEOUT = 8,        /* Unsupported, defined by Solaris.  */
	/* Treat ARG as `size_t *', and store there the TLS module ID
	   of this object's PT_TLS segment, as used in TLS relocations;
	   store zero if this object does not define a PT_TLS segment.  */
	RTLD_DI_TLS_MODID = 9,
	/* Treat ARG as `void **', and store there a pointer to the calling
	   thread's TLS block corresponding to this object's PT_TLS segment.
	   Store a null pointer if this object does not define a PT_TLS
	   segment, or if the calling thread has not allocated a block for it.  */
	RTLD_DI_TLS_DATA = 10,
	RTLD_DI_MAX = 10
};


typedef long int Lmid_t;
const Lmid_t LM_ID_BASE = 0;
const Lmid_t LM_ID_NEWLN = -1;

struct link_map {
	uintptr_t l_addr;  /* Difference between the address in the ELF file and the address in memory */
	char *l_name;  /* Absolute pathname where object was found */
	void *l_ld;    /* Dynamic section of the shared object */
	struct link_map *l_next, *l_prev; /* Chain of loaded objects */
};
}

extern "C" void _dl_resolve();
