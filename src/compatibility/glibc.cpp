#include "glibc.hpp"

#include <dlh/types.hpp>
#include <dlh/assert.hpp>
#include <dlh/utils/log.hpp>
#include <dlh/utils/auxiliary.hpp>

__attribute__ ((visibility("default"))) int __libc_enable_secure = 0;

// see https://github.com/jtracey/drow-loader/blob/master/glibc.c

char rtld_global[3992];
extern __attribute__ ((alias("rtld_global"), visibility("default"))) char * _rtld_global;

// see glibc (2.31) sysdeps/generic/ldsodefs.h
struct rtld_global_ro {

	/* If nonzero the appropriate debug information is printed.  */
	int _dl_debug_mask;

	/* OS version.  */
	unsigned int _dl_osversion;
	/* Platform name.  */
	const char *_dl_platform;
	size_t _dl_platformlen;

	/* Cached value of `getpagesize ()'.  */
	size_t _dl_pagesize;

	/* Do we read from ld.so.cache?  */
	int _dl_inhibit_cache;

	/* Copy of the content of `_dl_main_searchlist' at startup time.  */
	struct {
		/* Array of maps for the scope.  */
		/* struct link_map */ void **r_list;
		/* Number of entries in the scope.  */
		unsigned int r_nlist;
	} _dl_initial_searchlist;

	/* CLK_TCK as reported by the kernel.  */
	int _dl_clktck;

	/* If nonzero print warnings messages.  */
	int _dl_verbose;

	/* File descriptor to write debug messages to.  */
	int _dl_debug_fd;

	/* Do we do lazy relocations?  */
	int _dl_lazy;

	/* Nonzero if runtime lookups should not update the .got/.plt.  */
	int _dl_bind_not;

	/* Nonzero if references should be treated as weak during runtime
	   linking.  */
	int _dl_dynamic_weak;

	/* Default floating-point control word.  */
	unsigned int _dl_fpu_control;

	/* Expected cache ID.  */
	int _dl_correct_cache_id;

	/* Mask for hardware capabilities that are available.  */
	uint64_t _dl_hwcap;

	/* Mask for important hardware capabilities we honour. */
	uint64_t _dl_hwcap_mask;

	/* Pointer to the auxv list supplied to the program at startup.  */
	void *_dl_auxv;


	/* NB: When adding new fields, update sysdeps/x86/dl-diagnostics-cpu.c
	   to print them.  */
	struct cpu_features {
		struct cpu_features_basic {
			enum cpu_features_kind {
				arch_kind_unknown = 0,
				arch_kind_intel,
				arch_kind_amd,
				arch_kind_zhaoxin,
				arch_kind_other
			} kind;
			int max_cpuid;
			unsigned int family;
			unsigned int model;
			unsigned int stepping;
		} basic;
		struct cpuid_feature_internal {
			unsigned int cpuid_array[4];
			unsigned int usable_array[4];
		} features[1];
		unsigned int preferred[1];
		/* X86 micro-architecture ISA levels.  */
		unsigned int isa_1;
		/* The state size for XSAVEC or XSAVE.  The type must be unsigned long
		 int so that we use
		sub xsave_state_size_offset(%rip) %RSP_LP
		 in _dl_runtime_resolve.  */
		unsigned long int xsave_state_size;
		/* The full state size for XSAVE when XSAVEC is disabled by
		 GLIBC_TUNABLES=glibc.cpu.hwcaps=-XSAVEC
		*/
		unsigned int xsave_state_full_size;
		/* Data cache size for use in memory and string routines, typically
		 L1 size.  */
		unsigned long int data_cache_size;
		/* Shared cache size for use in memory and string routines, typically
		 L2 or L3 size.  */
		unsigned long int shared_cache_size;
		/* Threshold to use non temporal store.  */
		unsigned long int non_temporal_threshold;
		/* Threshold to use "rep movsb".  */
		unsigned long int rep_movsb_threshold;
		/* Threshold to stop using "rep movsb".  */
		unsigned long int rep_movsb_stop_threshold;
		/* Threshold to use "rep stosb".  */
		unsigned long int rep_stosb_threshold;
		/* _SC_LEVEL1_ICACHE_SIZE.  */
		unsigned long int level1_icache_size;
		/* _SC_LEVEL1_ICACHE_LINESIZE.  */
		unsigned long int level1_icache_linesize;
		/* _SC_LEVEL1_DCACHE_SIZE.  */
		unsigned long int level1_dcache_size;
		/* _SC_LEVEL1_DCACHE_ASSOC.  */
		unsigned long int level1_dcache_assoc;
		/* _SC_LEVEL1_DCACHE_LINESIZE.  */
		unsigned long int level1_dcache_linesize;
		/* _SC_LEVEL2_CACHE_ASSOC.  */
		unsigned long int level2_cache_size;
		/* _SC_LEVEL2_DCACHE_ASSOC.  */
		unsigned long int level2_cache_assoc;
		/* _SC_LEVEL2_CACHE_LINESIZE.  */
		unsigned long int level2_cache_linesize;
		/* /_SC_LEVEL3_CACHE_SIZE.  */
		unsigned long int level3_cache_size;
		/* _SC_LEVEL3_CACHE_ASSOC.  */
		unsigned long int level3_cache_assoc;
		/* _SC_LEVEL3_CACHE_LINESIZE.  */
		unsigned long int level3_cache_linesize;
		/* /_SC_LEVEL4_CACHE_SIZE.  */
		unsigned long int level4_cache_size;
	} _dl_x86_cpu_features;


	/* Names of shared object for which the RPATH should be ignored.  */
	const char *_dl_inhibit_rpath;

	/* Location of the binary.  */
	const char *_dl_origin_path;

	/* -1 if the dynamic linker should honor library load bias,
	   0 if not, -2 use the default (honor biases for normal
	   binaries, don't honor for PIEs).  */
	intptr_t _dl_use_load_bias;

	/* Name of the shared object to be profiled (if any).  */
	const char *_dl_profile;
	/* Filename of the output file.  */
	const char *_dl_profile_output;
	/* Name of the object we want to trace the prelinking.  */
	const char *_dl_trace_prelink;
	/* Map of shared object to be prelink traced.  */
	/* struct link_map */ void *_dl_trace_prelink_map;

	/* All search directories defined at startup.  This is assigned a
	   non-NULL pointer by the ld.so startup code (after initialization
	   to NULL), so this can also serve as an indicator whether a copy
	   of ld.so is initialized and active.  See the rtld_active function
	   below.  */
	/* struct r_search_path_elem */ void *_dl_init_all_dirs;


	/* The vsyscall page is a virtual DSO pre-mapped by the kernel.
	   This points to its ELF header.  */
	const /* ElfW(Ehdr) */ void *_dl_sysinfo_dso;

	/* At startup time we set up the normal DSO data structure for it,
	   and this points to it.  */
	/* struct link_map */ void *_dl_sysinfo_map;

	void * _dl_vdso_clock_gettime64;
	void * _dl_vdso_gettimeofday;
	void *_dl_vdso_time;
	void *_dl_vdso_getcpu;
	void *_dl_vdso_clock_getres_time64;
	/* Mask for more hardware capabilities that are available on some
	   platforms.  */
	uint64_t _dl_hwcap2;

	/* We add a function table to _rtld_global which is then used to
	   call the function instead of going through the PLT.  The result
	   is that we can avoid exporting the functions and we do not jump
	   PLT relocations in libc.so.  */
	void (*_dl_debug_printf) (const char *, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
	void (*_dl_mcount) (intptr_t frompc, uintptr_t selfpc);
	/* struct link_map */ void * (*_dl_lookup_symbol_x) (const char *, void *, const void **, void *[], const void *, int, int, void *);
	void *(*_dl_open) (const char *file, int mode, const void *caller_dlopen,
		     int nsid, int argc, char *argv[], char *env[]);
	void (*_dl_close) (void *map);
	void *(*_dl_tls_get_addr_soft) (void *);
	int (*_dl_discover_osversion) (void);

	/* List of auditing interfaces.  */
	/* struct audit_ifaces */ void *_dl_audit;
	unsigned int _dl_naudit;
} rtld_global_ro;
extern __attribute__((alias("rtld_global_ro"), visibility("default"))) struct rtld_global_ro _rtld_global_ro;

__attribute__ ((visibility("default"))) char **_dl_argv = nullptr;


void glibc_init() {
	rtld_global_ro._dl_pagesize = 4096;
	auto clktck = Auxiliary::vector(Auxiliary::AT_CLKTCK);
	assert(clktck.a_type == Auxiliary::AT_CLKTCK);
	rtld_global_ro._dl_clktck = clktck.value();

	cerr << "CLKTCK = " << rtld_global_ro._dl_clktck << endl;
}


#define CONFIG_RTLD_GLOBAL_SIZE 3992
#define CONFIG_RTLD_GLOBAL_RO_SIZE 536
#define CONFIG_RTLD_DL_PAGESIZE_OFFSET 24
#define CONFIG_RTLD_DL_CLKTCK_OFFSET 56
#define CONFIG_TCB_SIZE 2304
#define CONFIG_TCB_TCB_OFFSET 0
#define CONFIG_TCB_DTV_OFFSET 8
#define CONFIG_TCB_SELF_OFFSET 16
#define CONFIG_TCB_SYSINFO_OFFSET 32
#define CONFIG_TCB_STACK_GUARD 40
