#pragma once

#include <dlh/types.hpp>
#include <dlh/macro.hpp>

#include "comp/glibc/version.hpp"

typedef long int namespace_t;
const namespace_t NAMESPACE_BASE = 0;
const namespace_t NAMESPACE_NEW = -1;

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

struct Serinfo{
	size_t dls_size;           /* Size in bytes of the whole buffer */
	unsigned int dls_cnt;      /* Number of elements in 'dls_serpath' */
	struct Serpath {
		char *dls_name;            /* Name of library search path directory */
		unsigned int dls_flags;    /* Indicates where this directory came from */
	} dls_serpath[0];    /* Actually longer, 'dls_cnt' elements */
};

struct link_map {
	uintptr_t l_addr;  /* Difference between the address in the ELF file and the address in memory */
	char *l_name;  /* Absolute pathname where object was found */
	void *l_ld;    /* Dynamic section of the shared object */
	struct link_map *l_next, *l_prev; /* Chain of loaded objects */

	// GLIBC specific
	struct link_map *l_real;
	GLIBC::DL::Lmid_t l_ns;

	struct libname_list {
		const char *name;
		struct libname_list *next;
		int dont_free;
	} * l_libname;

#if GLIBC_VERSION >= GLIBC_2_36
	uintptr_t *l_info[80];
#elif GLIBC_VERSION >= GLIBC_2_28
	uintptr_t *l_info[77];
#else
	uintptr_t *l_info[76];
#endif

	const void *l_phdr;
	uintptr_t l_entry;
	uint16_t l_phnum;
	uint16_t l_ldnum;
	struct r_scope_elem  {
		struct link_map **r_list;
		unsigned r_nlist;
	} l_searchlist;
	struct r_scope_elem l_symbolic_searchlist;
	struct link_map *l_loader;
	void *l_versions;
	uint32_t l_nversions;
	uint32_t l_nbuckets;
	uint32_t l_gnu_bitmask_idxbits;
	uint32_t l_gnu_shift;
	const uint64_t *l_gnu_bitmask;
	union {
		const uint32_t *l_gnu_buckets;
		const uint32_t *l_chain;
	};
	union {
		const uint32_t *l_gnu_chain_zero;
		const uint32_t *l_buckets;
	};
	uint32_t l_direct_opencount;
	enum {
		lt_executable,
		lt_library,
		lt_loaded
	} l_type                           : 2;
#if GLIBC_VERSION >= GLIBC_2_36
	uint32_t l_dt_relr_ref             : 1;
#endif
	uint32_t l_relocated               : 1;
	uint32_t l_init_called             : 1;
	uint32_t l_global                  : 1;
	uint32_t l_reserved                : 2;
#if GLIBC_VERSION >= GLIBC_2_35 || defined(COMPATIBILITY_RHEL_9_LIKE)
	uint32_t l_main_map                : 1;
	uint32_t l_visited                 : 1;
	uint32_t l_map_used                : 1;
	uint32_t l_map_done                : 1;
#endif
	uint32_t l_phdr_allocated          : 1;
	uint32_t l_soname_added            : 1;
	uint32_t l_faked                   : 1;
	uint32_t l_need_tls_init           : 1;
	uint32_t l_auditing                : 1;
	uint32_t l_audit_any_plt           : 1;
	uint32_t l_removed                 : 1;
	uint32_t l_contiguous              : 1;
#if GLIBC_VERSION < GLIBC_2_36
	uint32_t l_symbolic_in_local_scope : 1;
#endif
	uint32_t l_free_initfini           : 1;
#if GLIBC_VERSION >= GLIBC_2_35 || defined(COMPATIBILITY_RHEL_9_LIKE)
	uint32_t l_ld_readonly             : 1;
#endif
#if GLIBC_VERSION >= GLIBC_2_35
	uint32_t l_find_object_processed   : 1;
#endif

#if (GLIBC_VERSION >= GLIBC_2_31 && !defined(COMPATIBILITY_DEBIAN_BUSTER)) || defined(COMPATIBILITY_RHEL_8_LIKE) || defined(COMPATIBILITY_RHEL_9_LIKE)
	bool l_nodelete_active;
	bool l_nodelete_pending;
#endif

#if GLIBC_VERSION >= GLIBC_2_33
	enum {
		lc_property_unknown = 0,        /* Unknown property status.  */
		lc_property_none = 1 << 0,      /* No property.  */
		lc_property_valid = 1 << 1      /* Has valid property.  */
	} l_property                       : 2;
	unsigned int l_x86_feature_1_and;
	unsigned int l_x86_isa_1_needed;
 #if GLIBC_VERSION >= GLIBC_2_35
	unsigned int l_1_needed;
 #endif
#elif GLIBC_VERSION >= GLIBC_2_28
	enum {
		lc_unknown = 0,                       /* Unknown CET status.  */
		lc_none    = 1 << 0,                  /* Not enabled with CET.  */
		lc_ibt     = 1 << 1,                  /* Enabled with IBT.  */
		lc_shstk   = 1 << 2,                  /* Enabled with STSHK.  */
		lc_ibt_and_shstk = lc_ibt | lc_shstk  /* Enabled with both.  */
	} l_cet                            : 3;
#endif

	struct {
		void **dirs;
		int malloced;
	} l_rpath_dirs;
	void *l_reloc_result;
	void *l_versyms;
	const char *l_origin;
	uintptr_t l_map_start;
	uintptr_t l_map_end;
	uintptr_t l_text_end;
	struct r_scope_elem *l_scope_mem[4];
	size_t l_scope_max;
	struct r_scope_elem **l_scope;
	struct r_scope_elem *l_local_scope[2];
	struct r_file_id {
		uint64_t dev;
		uint64_t ino;
	} l_file_id;
	struct r_search_path_struct {
		void **dirs;
		int malloced;
	} l_runpath_dirs;
	struct link_map **l_initfini;
	void *l_reldeps;
	uint32_t l_reldepsmax;
	uint32_t l_used;
	uint32_t l_feature_1;
	uint32_t l_flags_1;
	uint32_t l_flags;
	int l_idx;
	struct link_map_machine {
		uintptr_t plt;
		uintptr_t gotplt;
		void *tlsdesc_table;
	} l_mach;
	struct {
		const uintptr_t *sym;
		int type_class;
		struct link_map *value;
		const uintptr_t *ret;
	} l_lookup_cache;
	uintptr_t l_tls_initimage;
	size_t l_tls_initimage_size;
	size_t l_tls_blocksize;
	size_t l_tls_align;
	size_t l_tls_firstbyte_offset;
	ptrdiff_t l_tls_offset;
	size_t l_tls_modid;
	size_t l_tls_dtor_count;
	uintptr_t l_relro_addr;
	size_t l_relro_size;
	unsigned long long l_serial;
#if GLIBC_VERSION < GLIBC_2_31
	struct auditstate {
		uintptr_t cookie;
		unsigned int bindflags;
    } l_audit[0];
#endif
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
