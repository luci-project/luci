#include "compatibility/glibc.hpp"

#include <dlh/types.hpp>
#include <dlh/assert.hpp>
#include <dlh/unistd.hpp>
#include <dlh/utils/log.hpp>
#include <dlh/utils/auxiliary.hpp>

#include "loader.hpp"
#include "compatibility/patch.hpp"
#include "compatibility/dl.hpp"

__attribute__ ((visibility("default"))) int __libc_enable_secure = 0;


int dl_starting_up = 0;
extern __attribute__ ((alias("dl_starting_up"), visibility("default"))) int _dl_starting_up;

void *libc_stack_end = nullptr;
extern __attribute__ ((alias("libc_stack_end"), visibility("default"))) void * __libc_stack_end;

// see https://github.com/jtracey/drow-loader/blob/master/glibc.c

char rtld_global[3992];
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
	struct {
		/* Array of maps for the scope.  */
		/* struct link_map */ void **r_list;
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

	/* Nonzero if references should be treated as weak during runtime
	   linking.  */
	int _dl_dynamic_weak = 0 ;

	/* Default floating-point control word.  */
	unsigned int _dl_fpu_control = 0x037f;

	/* Expected cache ID.  */
	int _dl_correct_cache_id= 0;

	/* Mask for hardware capabilities that are available.  */
	uint64_t _dl_hwcap = 2;

	/* Pointer to the auxv list supplied to the program at startup.  */
	void *_dl_auxv = nullptr;


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
	const char *_dl_inhibit_rpath = nullptr;

	/* Location of the binary.  */
	const char *_dl_origin_path = nullptr;

	/* -1 if the dynamic linker should honor library load bias,
	   0 if not, -2 use the default (honor biases for normal
	   binaries, don't honor for PIEs).  */
	intptr_t _dl_use_load_bias = 0;

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

	/* Syscall handling improvements.  This is very specific to x86.  */
	uintptr_t _dl_sysinfo;

	/* The vsyscall page is a virtual DSO pre-mapped by the kernel.
	   This points to its ELF header.  */
	/* ElfW(Ehdr) */ void *_dl_sysinfo_dso;

	/* At startup time we set up the normal DSO data structure for it,
	   and this points to it.  */
	/* struct link_map */ void *_dl_sysinfo_map;

	void * _dl_vdso_clock_gettime64;
	void * _dl_vdso_gettimeofday;
	void * _dl_vdso_time;
	void * _dl_vdso_getcpu;
	void * _dl_vdso_clock_getres_time64;
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
	/* struct audit_ifaces */ void *_dl_audit = nullptr;
	unsigned int _dl_naudit = 0;
} rtld_global_ro;
extern __attribute__((alias("rtld_global_ro"), visibility("default"))) struct rtld_global_ro _rtld_global_ro;

__attribute__ ((visibility("default"))) char **_dl_argv = nullptr;


namespace GLIBC {

static void * resolve(const Loader & loader, const char * name) {
	auto sym = loader.resolve_symbol(name);
	return sym ? reinterpret_cast<void*>(sym->object().base + sym->value()) : nullptr;
}

void init_start(const Loader & loader) {
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
				rtld_global_ro._dl_platformlen = strlen(rtld_global_ro._dl_platform);
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

	if (sysinfo != 0 && rtld_global_ro._dl_sysinfo_dso != nullptr)
		rtld_global_ro._dl_sysinfo = sysinfo;

	rtld_global_ro._dl_vdso_clock_gettime64 = resolve(loader, "__vdso_clock_gettime");
	rtld_global_ro._dl_vdso_gettimeofday = resolve(loader, "__vdso_gettimeofday");
	rtld_global_ro._dl_vdso_time = resolve(loader, "__vdso_time");
	rtld_global_ro._dl_vdso_getcpu = resolve(loader, "__vdso_getcpu");
	rtld_global_ro._dl_vdso_clock_getres_time64 = resolve(loader, "__vdso_clock_getres");
}


static int _dl_addr_patch(void *address, DL::Info *info, void **mapp, __attribute__((unused)) const uintptr_t **symbolp) {
	return dladdr1(address, info, mapp, DL::RTLD_DL_LINKMAP);
}

static Patch fixes[] = {
	{ "_dl_addr", reinterpret_cast<uintptr_t>(_dl_addr_patch) },
	{ "__libc_dlopen_mode", reinterpret_cast<uintptr_t>(dlopen) },
	{ "__libc_dlclose", reinterpret_cast<uintptr_t>(dlclose) },
	{ "__libc_dlsym", reinterpret_cast<uintptr_t>(dlsym) },
	{ "__libc_dlvsym", reinterpret_cast<uintptr_t>(dlvsym) }
};

bool patch(const Elf::SymbolTable & symtab, uintptr_t base) {
	bool r = false;
	for (const auto & fix : fixes)
		if (fix.apply(symtab, base))
			r = true;
	return r;
}

void init_end() {
	dl_starting_up = 1;
}

void stack_end(void * ptr) {
	libc_stack_end = ptr;
}

}  // namespace GLIBC

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
