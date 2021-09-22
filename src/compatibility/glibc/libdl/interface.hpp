#pragma once

#include <dlh/types.hpp>

#include "object/identity.hpp"


namespace GLIBC {
namespace DL {

struct Info {
	const char *dli_fname;
	uintptr_t dli_fbase;
	const char *dli_sname;
	uintptr_t dli_saddr;
};

enum : int {
	/* Matching symbol table entry (const ElfNN_Sym *).  */
	RTLD_DL_SYMENT = 1,

	/* The object containing the address (struct link_map *).  */
	RTLD_DL_LINKMAP = 2
};

/* The MODE argument to `dlopen' contains one of the following: */
enum : unsigned {
	RTLD_LAZY     = 0x00001,	/* Lazy function call binding.  */
	RTLD_NOW      = 0x00002,	/* Immediate function call binding.  */
	RTLD_NOLOAD   = 0x00004,	/* Do not load the object.  */
	RTLD_DEEPBIND = 0x00008,	/* Use deep binding.  */
	/* If the following bit is set in the MODE argument to `dlopen',
	   the symbols of the loaded object and its dependencies are made
	   visible as if the object were linked directly into the program.  */
	RTLD_GLOBAL   = 0x00100,
	/* Unix98 demands the following flag which is the inverse to RTLD_GLOBAL.
	   The implementation does this by default and so we can define the
	   value to zero.  */
	RTLD_LOCAL    = 0,
	RTLD_NODELETE = 0x01000, /* Do not delete object when closed.  */
};


typedef long int Lmid_t;
static_assert(sizeof(Lmid_t) == sizeof(namespace_t), "Namespace has wrong type");
const Lmid_t LM_ID_BASE = 0;
const Lmid_t LM_ID_NEWLN = -1;
static_assert(LM_ID_BASE == NAMESPACE_BASE && LM_ID_NEWLN == NAMESPACE_NEW, "Namespace constants have wrong value");

struct link_map {
	uintptr_t l_addr;  /* Difference between the address in the ELF file and the address in memory */
	char *l_name;  /* Absolute pathname where object was found */
	void *l_ld;    /* Dynamic section of the shared object */
	struct link_map *l_next, *l_prev; /* Chain of loaded objects */
};

}  // namespace DL
}  // namespace GLIBC

extern "C" int dlclose(void *);
extern "C" const char *dlerror();
extern "C" void *dlopen(const char *, int);
extern "C" void *dlmopen (GLIBC::DL::Lmid_t, const char *, int);
extern "C" int dlinfo(void * __restrict, int, void * __restrict);
extern "C" void *dlsym(void *__restrict, const char *__restrict);
extern "C" void *dlvsym(void *__restrict, const char *__restrict, const char *__restrict );
extern "C" int dladdr(void *addr, GLIBC::DL::Info *info);
extern "C" int dladdr1(void *addr, GLIBC::DL::Info *info, void **extra_info, int flags);
