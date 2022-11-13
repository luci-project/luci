#pragma once

#include "loader.hpp"

#include <dlh/log.hpp>
#include <dlh/types.hpp>
#include <dlh/mutex_rec.hpp>

#include "tls.hpp"
#include "compatibility/gdb.hpp"
#include "compatibility/glibc/version.hpp"
#include "compatibility/glibc/libdl/interface.hpp"

extern "C" void _dl_debug_printf(const char *fmt, ...);

namespace GLIBC {
namespace RTLD {
void init_globals(const Loader & loader);
void init_globals_tls(const TLS & tls, void * dtv);
void stack_end(void * ptr);

// results using dwarfdump -n globals [system / ld]
struct Global {
	typedef char __rtld_lock_t[40];

	struct list_head {
	    struct list_head *next;
	    struct list_head *prev;
	};
	typedef list_head list_t;

	struct link_namespaces {
		GLIBC::DL::link_map *_ns_loaded;
		uint32_t _ns_nloaded;
		GLIBC::DL::link_map::r_scope_elem *_ns_main_searchlist;
		uint32_t _ns_global_scope_alloc;
		uint32_t _ns_global_scope_pending_adds;
#if GLIBC_VERSION >= GLIBC_2_32
		GLIBC::DL::link_map *libc_map;
#endif
		struct unique_sym_table  {
			__rtld_lock_t lock;
			void *entries;
			size_t size;
			size_t n_elements;
			void *free;
		} _ns_unique_sym_table;
#if GLIBC_VERSION >= GLIBC_2_35
		GDB::RDebugExtended _ns_debug;
#else
		GDB::RDebug _ns_debug;
#endif
	} _dl_ns[16];
	size_t _dl_nns = 1;
	__rtld_lock_t _dl_load_lock;
	__rtld_lock_t _dl_load_write_lock;
#if  GLIBC_VERSION >= GLIBC_2_35
	__rtld_lock_t _dl_load_tls_lock;
#endif
	uint64_t _dl_load_adds = 0;
	GLIBC::DL::link_map *_dl_initfirst = nullptr;
#if GLIBC_VERSION < GLIBC_2_30 || defined(COMPATIBILITY_DEBIAN_BUSTER)
	uint64_t _dl_cpuclock_offset;
#endif
	GLIBC::DL::link_map *_dl_profile_map = nullptr;
	uint64_t _dl_num_relocations;
	uint64_t _dl_num_cache_relocations;
	void *_dl_all_dirs;
#if  GLIBC_VERSION < GLIBC_2_25
	void *(*_dl_error_catch_tsd) ();
#endif
	GLIBC::DL::link_map _dl_rtld_map;
	struct auditstate {
		uintptr_t cookie;
		uint32_t bindflags;
	} _dl_rtld_auditstate[16] = {};

#if !GLIBC_PTHREAD_IN_LIBC
	void (*_dl_rtld_lock_recursive)(void *);
	void (*_dl_rtld_unlock_recursive)(void *);
#endif
#if GLIBC_VERSION >= GLIBC_2_32
	uint32_t _dl_x86_feature_1;
	struct dl_x86_feature_control {
		enum dl_x86_cet_control {
			cet_elf_property = 0,
			cet_always_on,
			cet_always_off,
			cet_permissive
		};
		enum dl_x86_cet_control ibt : 2;
		enum dl_x86_cet_control shstk : 2;
	} _dl_x86_feature_control;
#elif GLIBC_VERSION >= GLIBC_2_28
	uint32_t _dl_x86_feature_1[2] = {0, 0};
	uint64_t _dl_x86_legacy_bitmap[2] = {0, 0};
#endif
#if GLIBC_VERSION < GLIBC_2_34 || !GLIBC_PTHREAD_IN_LIBC
	int (*_dl_make_stack_executable_hook)(void**);
#endif
	uint32_t _dl_stack_flags = 0x6;
	bool _dl_tls_dtv_gaps = 0x0;
	size_t _dl_tls_max_dtv_idx = 0x1;
	void *_dl_tls_dtv_slotinfo_list;
	size_t _dl_tls_static_nelem = 0x1;
#if GLIBC_VERSION < GLIBC_2_34
	size_t _dl_tls_static_size = 0x1040;
#endif
	size_t _dl_tls_static_used = 0x90;
#if GLIBC_VERSION < GLIBC_2_34
	size_t _dl_tls_static_align = 0x40;
#endif
#if GLIBC_VERSION >= GLIBC_2_32 || defined(COMPATIBILITY_DEBIAN_BULLSEYE)
	size_t _dl_tls_static_optional;
#endif
	void *_dl_initial_dtv;
	size_t _dl_tls_generation = 0x1;
#if GLIBC_VERSION < GLIBC_2_34 || !GLIBC_PTHREAD_IN_LIBC
	void (*_dl_init_static_tls)(GLIBC::DL::link_map *);
#endif
#if GLIBC_VERSION < GLIBC_2_33
	void (*_dl_wait_lookup_done)();
#endif
	void *_dl_scope_free_list = 0;
#if GLIBC_PTHREAD_IN_LIBC && GLIBC_VERSION >= GLIBC_2_33
	list_t _dl_stack_used;
	list_t _dl_stack_user;
 #if GLIBC_VERSION >= GLIBC_2_34
	list_t _dl_stack_cache;
	size_t _dl_stack_cache_actsize;
	uintptr_t _dl_in_flight_stack;
 #endif
	int _dl_stack_cache_lock;
#elif !GLIBC_PTHREAD_IN_LIBC && GLIBC_VERSION >= GLIBC_2_35
	int _dl_pthread_num_threads;
	void * _dl_pthread_threads;
#endif
};

// see glibcsysdeps/generic/ldsodefs.h
struct GlobalRO {
	/* If nonzero the appropriate debug information is printed.  */
	int _dl_debug_mask = 0;

#if GLIBC_VERSION < GLIBC_2_36
	/* OS version.  */
	unsigned int _dl_osversion = 0;
#endif

	/* Platform name.  */
	const char *_dl_platform = "haswell";
	size_t _dl_platformlen = 7;

	/* Cached value of `getpagesize ()'.  */
	size_t _dl_pagesize = 0x1000;

#if GLIBC_VERSION >= GLIBC_2_34
	/* Cached value of `sysconf (_SC_MINSIGSTKSZ)'.  */
	size_t _dl_minsigstacksize = 1604;
#endif

	/* Do we read from ld.so.cache?  */
	int _dl_inhibit_cache = 0;

	/* Copy of the content of `_dl_main_searchlist' at startup time.  */
	struct r_scope_elem {
		/* Array of maps for the scope.  */
		void **r_list;
		/* Number of entries in the scope.  */
		unsigned int r_nlist;
	} _dl_initial_searchlist;

	/* CLK_TCK as reported by the kernel.  */
	int _dl_clktck = 0;

	/* If nonzero print warnings messages.  */
	int _dl_verbose = 0;

	/* File descriptor to write debug messages to.  */
	int _dl_debug_fd = 2;

	/* Do we do lazy relocations?  */
	int _dl_lazy = 1;

	/* Nonzero if runtime lookups should not update the .got/.plt.  */
	int _dl_bind_not = 0;

	/* Nonzero if references should be treated as weak during runtime linking.  */
	int _dl_dynamic_weak = 0 ;

	/* Default floating-point control word.  */
	unsigned short _dl_fpu_control = 0x037f;

	/* Expected cache ID.  */
	int _dl_correct_cache_id = 0;

	/* Mask for hardware capabilities that are available.  */
	uint64_t _dl_hwcap = 2;

#if GLIBC_VERSION < GLIBC_2_26 || (GLIBC_VERSION < GLIBC_2_26 && GLIBC_TUNABLE_COUNT == 0)
	/* Mask for important hardware capabilities we honour. */
	uint64_t _dl_hwcap_mask = 0;
#endif

	/* Pointer to the auxv list supplied to the program at startup.  */
	void *_dl_auxv = nullptr;

	/* CPU Feature information */
	struct cpu_features {
		enum cpu_features_kind {
			arch_kind_unknown = 0,
			arch_kind_intel = 1,
			arch_kind_amd =	2,
			arch_kind_other = 3
		};
		struct cpuid_registers {
			unsigned eax;
			unsigned ebx;
			unsigned ecx;
			unsigned edx;
		};

#if GLIBC_VERSION >= GLIBC_2_29 && !defined(COMPATIBILITY_DEBIAN_BUSTER)
 #if GLIBC_VERSION < GLIBC_2_32
		struct cpuid_registers cpuid[6];
		unsigned feature[2];
 #endif
		struct cpu_features_basic {
			enum cpu_features_kind kind;
			int max_cpuid;
			unsigned family;
			unsigned model;
			unsigned stepping;
		} basic;
 #if GLIBC_VERSION >= GLIBC_2_32
		struct cpuid_features {
			struct cpuid_registers cpuid;
			struct cpuid_registers usable;
		};
 #if GLIBC_VERSION >= GLIBC_2_34
		struct cpuid_features features[9];
 #elif GLIBC_VERSION == GLIBC_2_33
		struct cpuid_features features[8];
 #else
		struct cpuid_features features[7];
 #endif
		unsigned int preferred[1] = {4857};
 #endif
 #if GLIBC_VERSION >= GLIBC_2_33
		unsigned int isa_1 = 7;
 #endif
#else
		enum cpu_features_kind kind;
		int max_cpuid;
		struct cpuid_registers cpuid[3];
		unsigned family;
		unsigned model;
#endif

#if GLIBC_VERSION >= GLIBC_2_27 || defined(COMPATIBILITY_DEBIAN_STRETCH)
		unsigned long xsave_state_size = 960;
#endif
#if GLIBC_VERSION >= GLIBC_2_27
		unsigned xsave_state_full_size = 1152;
#endif
#if GLIBC_VERSION < GLIBC_2_29 || defined(COMPATIBILITY_DEBIAN_BUSTER)
		unsigned feature[1];
#endif
#if GLIBC_VERSION >= GLIBC_2_26
		unsigned long data_cache_size = 32768;
		unsigned long shared_cache_size = 1572864;
		unsigned long non_temporal_threshold = 0x120000;
#endif
#if GLIBC_VERSION >= GLIBC_2_33
		unsigned long rep_movsb_threshold = 0x2000;
 #if GLIBC_VERSION >= GLIBC_2_33
		unsigned long rep_movsb_stop_threshold = 0x120000;
 #endif
		unsigned long rep_stosb_threshold = 0x800;
		unsigned long level1_icache_size = 0x8000;
		unsigned long level1_icache_linesize = 64;
		unsigned long level1_dcache_size = 0x8000;
		unsigned long level1_dcache_assoc = 8;
		unsigned long level1_dcache_linesize = 64;
		unsigned long level2_cache_size = 0x40000;
		unsigned long level2_cache_assoc = 4;
		unsigned long level2_cache_linesize = 64;
		unsigned long level3_cache_size = 0x900000;
		unsigned long level3_cache_assoc = 12;
		unsigned long level3_cache_linesize = 64;
		unsigned long level4_cache_size = 0;
#endif
	} _dl_x86_cpu_features;
#if GLIBC_VERSION >= GLIBC_2_27
	const char _dl_x86_hwcap_flags[3][9] = { "sse2", "x86_64", "avx512_1" };
	const char _dl_x86_platforms[4][9] = { "i586", "i686", "haswell", "xeon_phi" };
#endif

	/* Names of shared object for which the RPATH should be ignored.  */
	const char *_dl_inhibit_rpath = nullptr;

	/* Location of the binary.  */
	const char *_dl_origin_path = nullptr;

#if GLIBC_VERSION < GLIBC_2_36
	/* -1 if the dynamic linker should honor library load bias,
	   0 if not, -2 use the default (honor biases for normal
	   binaries, don't honor for PIEs).  */
	uintptr_t _dl_use_load_bias = 0;
#endif

#if GLIBC_VERSION >= GLIBC_2_34
	/* Size of the static TLS block.  */
	size_t _dl_tls_static_size = 4224;

	/* Alignment requirement of the static TLS block.  */
	size_t _dl_tls_static_align = 64;
#endif
#if GLIBC_VERSION >= GLIBC_2_34 || defined(COMPATIBILITY_DEBIAN_BULLSEYE)
	/* Size of surplus space in the static TLS area for dynamically
	   loaded modules with IE-model TLS or for TLSDESC optimization.
	   See comments in elf/dl-tls.c where it is initialized.  */
	size_t _dl_tls_static_surplus = 1664;
#endif

	/* Name of the shared object to be profiled (if any).  */
	const char *_dl_profile = 0;
	/* Filename of the output file.  */
	const char *_dl_profile_output = "/var/tmp";

#if GLIBC_VERSION < GLIBC_2_36
	/* Name of the object we want to trace the prelinking.  */
	const char *_dl_trace_prelink = nullptr;
	/* Map of shared object to be prelink traced.  */
	/* struct link_map */ void *_dl_trace_prelink_map = nullptr;
#endif

	/* All search directories defined at startup.  This is assigned a
	   non-NULL pointer by the ld.so startup code (after initialization
	   to NULL), so this can also serve as an indicator whether a copy
	   of ld.so is initialized and active.  See the rtld_active function
	   below.  */
	/* struct r_search_path_elem */ void *_dl_init_all_dirs = nullptr;

	/* The vsyscall page is a virtual DSO pre-mapped by the kernel.
	   This points to its ELF header.  */
	/* ElfW(Ehdr) */ void *_dl_sysinfo_dso = nullptr;

	/* At startup time we set up the normal DSO data structure for it,
	   and this points to it.  */
	/* struct link_map */ void *_dl_sysinfo_map = nullptr;

#if GLIBC_VERSION >= GLIBC_2_31 && !defined(COMPATIBILITY_DEBIAN_BUSTER)
	void * _dl_vdso_clock_gettime64 = nullptr;
	void * _dl_vdso_gettimeofday = nullptr;
	void * _dl_vdso_time = nullptr;
	void * _dl_vdso_getcpu = nullptr;
	void * _dl_vdso_clock_getres_time64 = nullptr;
#endif

	/* Mask for more hardware capabilities that are available on some platforms.  */
	uint64_t _dl_hwcap2 = 2;

#if GLIBC_VERSION >= GLIBC_2_34
	enum dso_sort_algorithm {
		dso_sort_algorithm_original,
		dso_sort_algorithm_dfs
	} _dl_dso_sort_algo = dso_sort_algorithm_dfs;
#endif


	/* We add a function table to _rtld_global which is then used to
	   call the function instead of going through the PLT.  The result
	   is that we can avoid exporting the functions and we do not jump
	   PLT relocations in libc.so.  */
	void (*_dl_debug_printf) (const char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
#if GLIBC_VERSION < GLIBC_2_25
	int (*_dl_catch_error) (const char **, const char **,  bool *, void (*) (void *), void *);
	void (*_dl_signal_error) (int, const char *, const char *,const char *);
#endif
	void (*_dl_mcount) (uintptr_t, uintptr_t);
	void * (*_dl_lookup_symbol_x) (const char *, GLIBC::DL::link_map *, const void **, void *[], const void *, int, int,  GLIBC::DL::link_map *);
#if GLIBC_VERSION < GLIBC_2_28
	int (*_dl_check_caller) (const void *, int);
#endif
	void *(*_dl_open) (const char *, int, const void *, GLIBC::DL::Lmid_t, int, char **, char **);
	void (*_dl_close) (void *);
#if GLIBC_VERSION >= GLIBC_2_34
	int (*_dl_catch_error)(const char **, const char **, bool *, void (*)(void *), void *);
	void (*_dl_error_free)(void *);
#endif
	void *(*_dl_tls_get_addr_soft) (GLIBC::DL::link_map *);
#if GLIBC_VERSION >= GLIBC_2_35
	void (*_dl_libc_freeres)(void);
	int (*_dl_find_object)(void *, void *);
#endif
#if GLIBC_VERSION < GLIBC_2_36
	int (*_dl_discover_osversion) (void);
#endif
#if GLIBC_VERSION >= GLIBC_2_34
	const struct dlfcn_hook *_dl_dlfcn_hook = 0;
#endif
	/* List of auditing interfaces.  */
	/* struct audit_ifaces */ void *_dl_audit = nullptr;
	unsigned int _dl_naudit = 0;
};
}  // namespace RTLD
}  // nammespace GLIBC

extern "C" GLIBC::RTLD::Global rtld_global;
extern "C" GLIBC::RTLD::GlobalRO rtld_global_ro;
