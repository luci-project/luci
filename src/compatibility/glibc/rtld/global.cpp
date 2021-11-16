#include "compatibility/glibc/rtld/global.hpp"

#include <dlh/types.hpp>
#include <dlh/assert.hpp>
#include <dlh/string.hpp>
#include <dlh/auxiliary.hpp>

// results using dwarfdump -n globals [system / ld]
char rtld_global[
#if defined(COMPATIBILITY_DEBIAN_STRETCH_AMD64)
	3968
#elif defined(COMPATIBILITY_UBUNTU_FOCAL_AMD64)
	3992
#else
	0
#endif
];
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

#if defined(COMPATIBILITY_DEBIAN_STRETCH_AMD64)
	/* Mask for important hardware capabilities we honour. */
	uint64_t _dl_hwcap_mask = 0;
#endif

	/* Pointer to the auxv list supplied to the program at startup.  */
	void *_dl_auxv = nullptr;

#if defined(COMPATIBILITY_DEBIAN_STRETCH_AMD64)
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
#elif defined(COMPATIBILITY_UBUNTU_FOCAL_AMD64)
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
	const char *_dl_profile_output;
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
	/* struct link_map */ void *_dl_sysinfo_map;

#if defined(COMPATIBILITY_UBUNTU_FOCAL_AMD64)
	void * _dl_vdso_clock_gettime64;
	void * _dl_vdso_gettimeofday;
	void * _dl_vdso_time;
	void * _dl_vdso_getcpu;
	void * _dl_vdso_clock_getres_time64;
#endif

	/* Mask for more hardware capabilities that are available on some
	   platforms.  */
	uint64_t _dl_hwcap2;

	/* We add a function table to _rtld_global which is then used to
	   call the function instead of going through the PLT.  The result
	   is that we can avoid exporting the functions and we do not jump
	   PLT relocations in libc.so.  */
	void (*_dl_debug_printf) (const char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
#if defined(COMPATIBILITY_DEBIAN_STRETCH_AMD64)
	int (*_dl_catch_error) (const char **, const char **,  bool *, void (*) (void *), void *);
	void (*_dl_signal_error) (int, const char *, const char *,const char *);
#endif
	void (*_dl_mcount) (intptr_t frompc, uintptr_t selfpc);
	void * (*_dl_lookup_symbol_x) (const char *, void *, const void **, void *[], const void *, int, int, void *);
#if defined(COMPATIBILITY_DEBIAN_STRETCH_AMD64)
	int (*_dl_check_caller) (const void *, int);
#endif
	void *(*_dl_open) (const char *file, int mode, const void *caller_dlopen, int nsid, int argc, char *argv[], char *env[]);
	void (*_dl_close) (void *map);
	void *(*_dl_tls_get_addr_soft) (void *);
	int (*_dl_discover_osversion) (void);

	/* List of auditing interfaces.  */
	/* struct audit_ifaces */ void *_dl_audit = nullptr;
	unsigned int _dl_naudit = 0;
} rtld_global_ro;
#if defined(COMPATIBILITY_DEBIAN_STRETCH_AMD64)
static_assert(sizeof(rtld_global_ro) == 376, "Wrong size of rtld_global_ro for Debian Stretch (amd64)");
#elif defined(COMPATIBILITY_UBUNTU_FOCAL_AMD64)
static_assert(sizeof(rtld_global_ro) == 536, "Wrong size of rtld_global_ro for Ubuntu Focal (amd64)");
#else
#error No (known) compatibility mode specified
#endif

extern __attribute__((alias("rtld_global_ro"), visibility("default"))) struct rtld_global_ro _rtld_global_ro;

__attribute__ ((visibility("default"))) int __libc_enable_secure = 0;

void *libc_stack_end = nullptr;
extern __attribute__ ((alias("libc_stack_end"), visibility("default"))) void * __libc_stack_end;


namespace GLIBC {
namespace RTLD {

static void * resolve(const Loader & loader, const char * name) {
	auto sym = loader.resolve_symbol(name);
	return sym ? reinterpret_cast<void*>(sym->object().base + sym->value()) : nullptr;
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

#if defined(COMPATIBILITY_UBUNTU_FOCAL_AMD64)
	rtld_global_ro._dl_vdso_clock_gettime64 = resolve(loader, "__vdso_clock_gettime");
	rtld_global_ro._dl_vdso_gettimeofday = resolve(loader, "__vdso_gettimeofday");
	rtld_global_ro._dl_vdso_time = resolve(loader, "__vdso_time");
	rtld_global_ro._dl_vdso_getcpu = resolve(loader, "__vdso_getcpu");
	rtld_global_ro._dl_vdso_clock_getres_time64 = resolve(loader, "__vdso_clock_getres");
#endif
}


void stack_end(void * ptr) {
	libc_stack_end = ptr;
}

}  // namespace RTLD
}  // namespace GLIBC
