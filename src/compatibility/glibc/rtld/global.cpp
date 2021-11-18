#include "compatibility/glibc/rtld/global.hpp"

#include <dlh/types.hpp>
#include <dlh/assert.hpp>
#include <dlh/string.hpp>
#include <dlh/thread.hpp>
#include <dlh/auxiliary.hpp>

#include "compatibility/glibc/libdl/interface.hpp"
#include "compatibility/gdb.hpp"

typedef char __rtld_lock_t[40];

// results using dwarfdump -n globals [system / ld]
struct rtld_global {
	struct link_namespaces {
		GLIBC::DL::link_map *_ns_loaded;	/* offset 0 */
		uint32_t _ns_nloaded;	/* offset 8 */
		GLIBC::DL::link_map::r_scope_elem *_ns_main_searchlist;	/* offset 16 */
		uint32_t _ns_global_scope_alloc;	/* offset 24 */
		uint32_t _ns_global_scope_pending_adds;	/* offset 28 */
		struct unique_sym_table  {
			__rtld_lock_t lock;	/* offset 0 */
			void *entries;	/* offset 40 */
			size_t size;		/* offset 48 */
			size_t n_elements;	/* offset 56 */
			void *free;		/* offset 64 */
		} _ns_unique_sym_table;	/* offset 32 */
		GDB::RDebug _ns_debug;
	} _dl_ns[16];			/* offset 0 */
	size_t _dl_nns;		/* offset 2304 */
	__rtld_lock_t _dl_load_lock;	/* offset 2312 */
	__rtld_lock_t _dl_load_write_lock;	/* offset 2352 */
	uint64_t _dl_load_adds;	/* offset 2392 */
	GLIBC::DL::link_map *_dl_initfirst;	/* offset 2400 */
#if defined(COMPATIBILITY_DEBIAN_STRETCH_X64)
	uint64_t _dl_cpuclock_offset;	/* offset 2408 */
#endif
	GLIBC::DL::link_map *_dl_profile_map;	/* offset 2416 */
	uint64_t _dl_num_relocations;	/* offset 2424 */
	uint64_t _dl_num_cache_relocations;	/* offset 2432 */
	void *_dl_all_dirs;	/* offset 2440 */
#if defined(COMPATIBILITY_DEBIAN_STRETCH_X64)
	void *(*_dl_error_catch_tsd) ();	/* offset 2448 */
#endif
	GLIBC::DL::link_map _dl_rtld_map;
	struct auditstate {
		uintptr_t cookie;		/* offset 0 */
		uint32_t bindflags;	/* offset 8 */
	} _dl_rtld_auditstate[16];

	void (*_dl_rtld_lock_recursive)(void *);	/* offset 3848 */
	void (*_dl_rtld_unlock_recursive)(void *);	/* offset 3856 */
#if defined(COMPATIBILITY_UBUNTU_FOCAL_X64)
	uint32_t _dl_x86_feature_1[2];	/* offset 3864 */
	uint64_t _dl_x86_legacy_bitmap[2];	/* offset 3872 */
#endif
	int (*_dl_make_stack_executable_hook)(void**);	/* offset 3888 */
	uint32_t _dl_stack_flags;	/* offset 3896 */
	uint32_t _dl_tls_dtv_gaps;	/* offset 3900 */
	size_t _dl_tls_max_dtv_idx;	/* offset 3904 */
	void *_dl_tls_dtv_slotinfo_list;	/* offset 3912 */
	size_t _dl_tls_static_nelem;	/* offset 3920 */
	size_t _dl_tls_static_size;	/* offset 3928 */
	size_t _dl_tls_static_used;	/* offset 3936 */
	size_t _dl_tls_static_align;	/* offset 3944 */
	void *_dl_initial_dtv;	/* offset 3952 */
	size_t _dl_tls_generation;	/* offset 3960 */
	void (*_dl_init_static_tls)(GLIBC::DL::link_map *);	/* offset 3968 */
	void (*_dl_wait_lookup_done)();	/* offset 3976 */
	void *_dl_scope_free_list;	/* offset 3984 */
} rtld_global;

#if defined(COMPATIBILITY_DEBIAN_STRETCH_X64)
static_assert(sizeof(rtld_global) == 3968, "Wrong size of rtld_globalo for Debian Stretch (amd64)");
#elif defined(COMPATIBILITY_UBUNTU_FOCAL_X64)
static_assert(sizeof(rtld_global) == 3992, "Wrong size of rtld_globalo for Ubuntu Focal (amd64)");
#else
#error No (known) rtld_global compatibility mode specified
#endif

extern __attribute__ ((alias("rtld_global"), visibility("default"))) char * _rtld_global;

// see glibc (2.31) sysdeps/generic/ldsodefs.h
struct rtld_global_ro {

	/* If nonzero the appropriate debug information is printed.  */
	int _dl_debug_mask = 0;

	/* OS version.  */
	unsigned int _dl_osversion = 0;
	/* Platform name.  */
	const char *_dl_platform = "";
	size_t _dl_platformlen = 0;

	/* Cached value of `getpagesize ()'.  */
	size_t _dl_pagesize = 0x1000;

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

#if defined(COMPATIBILITY_DEBIAN_STRETCH_X64)
	/* Mask for important hardware capabilities we honour. */
	uint64_t _dl_hwcap_mask = 0;
#endif

	/* Pointer to the auxv list supplied to the program at startup.  */
	void *_dl_auxv = nullptr;

#if defined(COMPATIBILITY_DEBIAN_STRETCH_X64)
	struct cpu_features {
		enum cpu_features_kind {
			arch_kind_unknown = 0,
			arch_kind_intel = 1,
			arch_kind_amd =	2,
			arch_kind_other = 3
		} kind;
		int max_cpuid;
		struct cpuid_registers {
			unsigned eax;
			unsigned ebx;
			unsigned ecx;
			unsigned edx;
		} cpuid[3];
		unsigned family;
		unsigned model;
		unsigned long xsave_state_size;
		unsigned feature;
	} _dl_x86_cpu_features;
#elif defined(COMPATIBILITY_UBUNTU_FOCAL_X64)
 	struct cpu_features {
		struct cpuid_registers {
			unsigned eax;
			unsigned ebx;
			unsigned ecx;
			unsigned edx;
		} cpuid[6];
		unsigned feature[2];
		struct cpu_features_basic {
			enum cpu_features_kind {
				arch_kind_unknown = 0,
				arch_kind_intel = 1,
				arch_kind_amd = 2,
				arch_kind_other = 3
			} kind;
			int max_cpuid;
			unsigned family;
			unsigned model;
			unsigned stepping;
		} basic;
		unsigned long xsave_state_size;
		unsigned xsave_state_full_size;
		unsigned long data_cache_size;
		unsigned long shared_cache_size;
		unsigned long non_temporal_threshold;
	} _dl_x86_cpu_features;
	const char _dl_x86_hwcap_flags[3][9] = { "sse2", "x86_64", "avx512_1" };
	const char _dl_x86_platforms[4][9] = { "i586", "i686", "haswell", "xeon_phi" };
#endif

	/* Names of shared object for which the RPATH should be ignored.  */
	const char *_dl_inhibit_rpath = nullptr;

	/* Location of the binary.  */
	const char *_dl_origin_path = nullptr;

	/* -1 if the dynamic linker should honor library load bias,
	   0 if not, -2 use the default (honor biases for normal
	   binaries, don't honor for PIEs).  */
	uintptr_t _dl_use_load_bias = 0;

	/* Name of the shared object to be profiled (if any).  */
	const char *_dl_profile = 0;
	/* Filename of the output file.  */
	const char *_dl_profile_output = "/var/tmp";
	/* Name of the object we want to trace the prelinking.  */
	const char *_dl_trace_prelink = nullptr;
	/* Map of shared object to be prelink traced.  */
	/* struct link_map */ void *_dl_trace_prelink_map = nullptr;

	/* All search directories defined at startup.  This is assigned a
	   non-NULL pointer by the ld.so startup code (after initialization
	   to NULL), so this can also serve as an indicator whether a copy
	   of ld.so is initialized and active.  See the rtld_active function
	   below.  */
	/* struct r_search_path_elem */ void *_dl_init_all_dirs = nullptr;

	/* The vsyscall page is a virtual DSO pre-mapped by the kernel.
	   This points to its ELF header.  */
	/* ElfW(Ehdr) */ void *_dl_sysinfo_dso;

	/* At startup time we set up the normal DSO data structure for it,
	   and this points to it.  */
	/* struct link_map */ void *_dl_sysinfo_map = nullptr;

#if defined(COMPATIBILITY_UBUNTU_FOCAL_X64)
	void * _dl_vdso_clock_gettime64;
	void * _dl_vdso_gettimeofday;
	void * _dl_vdso_time;
	void * _dl_vdso_getcpu;
	void * _dl_vdso_clock_getres_time64;
#endif

	/* Mask for more hardware capabilities that are available on some platforms.  */
	uint64_t _dl_hwcap2;

	/* We add a function table to _rtld_global which is then used to
	   call the function instead of going through the PLT.  The result
	   is that we can avoid exporting the functions and we do not jump
	   PLT relocations in libc.so.  */
	void (*_dl_debug_printf) (const char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
#if defined(COMPATIBILITY_DEBIAN_STRETCH_X64)
	int (*_dl_catch_error) (const char **, const char **,  bool *, void (*) (void *), void *);
	void (*_dl_signal_error) (int, const char *, const char *,const char *);
#endif
	void (*_dl_mcount) (intptr_t, uintptr_t);
	void * (*_dl_lookup_symbol_x) (const char *, void *, const void **, void *[], const void *, int, int, void *);
#if defined(COMPATIBILITY_DEBIAN_STRETCH_X64)
	int (*_dl_check_caller) (const void *, int);
#endif
	void *(*_dl_open) (const char *, int, const void *, int, int, char **, char **);
	void (*_dl_close) (void *);
	void *(*_dl_tls_get_addr_soft) (void *);
	int (*_dl_discover_osversion) (void);

	/* List of auditing interfaces.  */
	/* struct audit_ifaces */ void *_dl_audit = nullptr;
	unsigned int _dl_naudit = 0;
} rtld_global_ro;
#if defined(COMPATIBILITY_DEBIAN_STRETCH_X64)
static_assert(sizeof(rtld_global_ro) == 376, "Wrong size of rtld_global_ro for Debian Stretch (amd64)");
#elif defined(COMPATIBILITY_UBUNTU_FOCAL_X64)
static_assert(sizeof(rtld_global_ro) == 536, "Wrong size of rtld_global_ro for Ubuntu Focal (amd64)");
#else
#error No (known) rtld_global_ro compatibility mode specified
#endif

extern __attribute__((alias("rtld_global_ro"), visibility("default"))) struct rtld_global_ro _rtld_global_ro;

__attribute__ ((visibility("default"))) int __libc_enable_secure = 0;

void *libc_stack_end = nullptr;
extern __attribute__ ((alias("libc_stack_end"), visibility("default"))) void * __libc_stack_end;


namespace GLIBC {
namespace RTLD {

#if defined(COMPATIBILITY_UBUNTU_FOCAL_X64)
static void * resolve(const Loader & loader, const char * name) {
	auto sym = loader.resolve_symbol(name);
	return sym ? reinterpret_cast<void*>(sym->object().base + sym->value()) : nullptr;
}
#elif defined(COMPATIBILITY_DEBIAN_STRETCH_X64)
static void * error_catch_tsd() {
	LOG_INFO << "Using error_catch_tsd!" << endl;
	//return &(Thread::self()->__glibc_unused2);
	static void * data;
	return &data; // TODO: should be __thread
}
#endif

static void notimplemented() {
	LOG_ERROR << "This function was not implemented" << endl;
}

void init_globals(const Loader & loader) {
	uintptr_t sysinfo = 0;
	Auxiliary * auxv = Auxiliary::begin();
	rtld_global_ro._dl_auxv = auxv;
	for (int auxc = 0 ; auxv[auxc].valid(); auxc++) {
		const auto & aux = auxv[auxc];
		switch (aux.a_type) {
			case Auxiliary::AT_CLKTCK:
				rtld_global_ro._dl_clktck = aux.value();
				break;
			case Auxiliary::AT_PAGESZ:
				rtld_global_ro._dl_pagesize = aux.value();
				break;
			case Auxiliary::AT_PLATFORM:
				rtld_global_ro._dl_platform = reinterpret_cast<char*>(aux.pointer());
				rtld_global_ro._dl_platformlen = String::len(rtld_global_ro._dl_platform);
				break;
			case Auxiliary::AT_HWCAP:
				rtld_global_ro._dl_hwcap = aux.value();
				break;
			case Auxiliary::AT_HWCAP2:
				rtld_global_ro._dl_hwcap2 = aux.value();
				break;
			case Auxiliary::AT_FPUCW:
				rtld_global_ro._dl_fpu_control = aux.value();
				break;
			case Auxiliary::AT_SYSINFO:
				sysinfo = aux.value();
				break;
			case Auxiliary::AT_SYSINFO_EHDR:
				rtld_global_ro._dl_sysinfo_dso = aux.pointer();
				break;
		}
	}
	(void) sysinfo;
	(void) loader;

	rtld_global._dl_rtld_lock_recursive = reinterpret_cast<void (*)(void *)>(notimplemented);
	rtld_global._dl_rtld_unlock_recursive = reinterpret_cast<void (*)(void *)>(notimplemented);
	rtld_global._dl_make_stack_executable_hook = reinterpret_cast<int (*)(void **)>(notimplemented);
	rtld_global._dl_init_static_tls = reinterpret_cast<	void (*)(GLIBC::DL::link_map *)>(notimplemented);
	rtld_global._dl_wait_lookup_done = reinterpret_cast<	void (*)()>(notimplemented);

	rtld_global_ro._dl_debug_printf = reinterpret_cast<void (*) (const char *, ...)>(notimplemented);
	rtld_global_ro._dl_mcount = reinterpret_cast<void (*) (intptr_t, uintptr_t)>(notimplemented);
	rtld_global_ro._dl_lookup_symbol_x = reinterpret_cast<void * (*) (const char *, void *, const void **, void *[], const void *, int, int, void *)>(notimplemented);
	rtld_global_ro._dl_open = reinterpret_cast<void *(*) (const char *, int, const void *, int, int, char **, char **)>(notimplemented);
	rtld_global_ro._dl_close = reinterpret_cast<void (*) (void *)>(notimplemented);
	rtld_global_ro._dl_tls_get_addr_soft = reinterpret_cast<void *(*) (void *)>(notimplemented);
	rtld_global_ro._dl_discover_osversion = reinterpret_cast<int (*) (void)>(notimplemented);

#if defined(COMPATIBILITY_UBUNTU_FOCAL_X64)
	rtld_global_ro._dl_vdso_clock_gettime64 = resolve(loader, "__vdso_clock_gettime");
	rtld_global_ro._dl_vdso_gettimeofday = resolve(loader, "__vdso_gettimeofday");
	rtld_global_ro._dl_vdso_time = resolve(loader, "__vdso_time");
	rtld_global_ro._dl_vdso_getcpu = resolve(loader, "__vdso_getcpu");
	rtld_global_ro._dl_vdso_clock_getres_time64 = resolve(loader, "__vdso_clock_getres");
#elif defined(COMPATIBILITY_DEBIAN_STRETCH_X64)
	rtld_global._dl_error_catch_tsd = error_catch_tsd;

	rtld_global_ro._dl_catch_error = reinterpret_cast<int (*) (const char **, const char **,  bool *, void (*) (void *), void *)>(notimplemented);
	rtld_global_ro._dl_signal_error = reinterpret_cast<void (*) (int, const char *, const char *,const char *)>(notimplemented);
	rtld_global_ro._dl_check_caller = reinterpret_cast<int (*) (const void *, int)>(notimplemented);
#endif
}


void stack_end(void * ptr) {
	libc_stack_end = ptr;
}

}  // namespace RTLD
}  // namespace GLIBC
