#include "comp/glibc/rtld/global.hpp"

#include <dlh/log.hpp>
#include <dlh/mem.hpp>
#include <dlh/math.hpp>
#include <dlh/types.hpp>
#include <dlh/assert.hpp>
#include <dlh/string.hpp>
#include <dlh/thread.hpp>
#include <dlh/mutex_rec.hpp>
#include <dlh/auxiliary.hpp>

#include "comp/glibc/rtld/dl.hpp"
#include "comp/glibc/rtld/dl_tls.hpp"
#include "comp/glibc/rtld/exception.hpp"

/* Setup _rtld_global */
GLIBC::RTLD::Global rtld_global;
extern __attribute__ ((alias("rtld_global"), visibility("default"))) GLIBC::RTLD::Global _rtld_global;

/* Setup _rtld_global_ro */
GLIBC::RTLD::GlobalRO rtld_global_ro;
extern __attribute__((alias("rtld_global_ro"), visibility("default"))) GLIBC::RTLD::GlobalRO _rtld_global_ro;

EXPORT const GLIBC::RTLD::GlobalRO::cpu_features * _dl_x86_get_cpu_features() {
	return &(rtld_global_ro._dl_x86_cpu_features);
}

EXPORT const GLIBC::RTLD::GlobalRO::cpu_features * __get_cpu_features() {
	return &(rtld_global_ro._dl_x86_cpu_features);
}

__attribute__ ((visibility("default"))) int __libc_enable_secure = 0;

void *libc_stack_end = nullptr;
extern __attribute__ ((alias("libc_stack_end"), visibility("default"))) void * __libc_stack_end;

void *dlfcn_hook = nullptr;
extern __attribute__ ((alias("dlfcn_hook"), visibility("default"))) void * _dlfcn_hook;

__attribute__ ((visibility("default"))) unsigned int __rseq_flags = 0;
__attribute__ ((visibility("default"))) unsigned int __rseq_size = 0;
__attribute__ ((visibility("default"))) ptrdiff_t __rseq_offset = 0;


namespace GLIBC {
namespace RTLD {

#if GLIBC_VERSION < GLIBC_2_25
static __attribute__((unused)) void * _dl_error_catch_tsd() {
	LOG_INFO << "Using error_catch_tsd!" << endl;
	//return &(Thread::self()->__glibc_unused2);
	static void * data;
	return &data; // TODO: should be __thread
}
#endif

#if !GLIBC_PTHREAD_IN_LIBC
static MutexRecursive _dl_rtld_mutex;
static __attribute__((unused)) void _dl_rtld_lock_recursive(void* arg) {
	LOG_DEBUG << "RTLD lock " << arg << endl;
	if (!_dl_rtld_mutex.lock())
		LOG_ERROR << "RTLD lock " << arg << "failed!" << endl;
}

static __attribute__((unused)) void _dl_rtld_unlock_recursive(void* arg) {
	LOG_DEBUG << "RTLD unlock " << arg << endl;
	_dl_rtld_mutex.unlock();
}
#endif


#if GLIBC_VERSION < GLIBC_2_34 || !GLIBC_PTHREAD_IN_LIBC
static __attribute__((unused)) void _dl_init_static_tls(GLIBC::DL::link_map * map) {
	(void) map;
	LOG_ERROR << "GLIBC _dl_init_static_tls not implemented" << endl;
}
#endif

#if GLIBC_VERSION < GLIBC_2_33
static __attribute__((unused)) void _dl_wait_lookup_done() {
	LOG_ERROR << "GLIBC _dl_wait_lookup_done not implemented" << endl;
}
#endif

EXPORT void _dl_debug_printf(const char *fmt, ...) {
	va_list arg;
	va_start (arg, fmt);
	LOG_DEBUG.output(fmt, arg);
	va_end (arg);
}

static __attribute__((unused)) void * _dl_lookup_symbol_x(const char * undef_name, GLIBC::DL::link_map *undef_map, const void **ref, void *symbol_scope[], const void * version, int type_class, int flags, GLIBC::DL::link_map * skip_map){
	(void) undef_map;
	(void) ref;
	(void) symbol_scope;
	(void) version;
	(void) type_class;
	(void) flags;
	(void) skip_map;
	LOG_ERROR << "GLIBC _dl_lookup_symbol_x (for " << undef_name << ") not implemented" << endl;
	return nullptr;
}

static __attribute__((unused)) void * _dl_open(const char *file, int mode, const void *caller_dlopen, GLIBC::DL::Lmid_t nsid, int argc, char *argv[], char *env[]) {
	(void) mode;
	(void) caller_dlopen;
	(void) nsid;
	(void) argc;
	(void) argv;
	(void) env;
	LOG_ERROR << "GLIBC _dl_open (for " << file << ") not implemented" << endl;
	return nullptr;
}

static __attribute__((unused)) void _dl_close(void *) {
	LOG_ERROR << "GLIBC _dl_close not implemented" << endl;
}

#if GLIBC_VERSION >= GLIBC_2_35
static __attribute__((unused)) void _dl_libc_freeres() {
	LOG_ERROR << "GLIBC _dl_libc_freeres not implemented" << endl;
}

static int _dl_find_object(uintptr_t address, GLIBC::RTLD::GlobalRO::dl_find_object * result) {
	LOG_TRACE << "GLIBC _dl_find_object(" << (void*)address << ", " << result << ")" << endl;
	auto loader = Loader::instance();
	assert(loader != nullptr);

	GuardedReader _{loader->lookup_sync};
	for (const auto & object_file : loader->lookup)
		for (Object * obj = object_file.current; obj != nullptr; obj = obj->file_previous)
			for (const MemorySegment & mem : obj->memory_map)
				if (mem.target.contains(address)) {
					result->dlfo_flags = 0;
					result->dlfo_map_start = mem.target.address();
					result->dlfo_map_end = mem.target.address() + mem.target.size;
					result->dlfo_link_map = &object_file.glibc_link_map;
					result->dlfo_eh_frame = 0;
					for (const auto & s : obj->Elf::segments)
						if (s.type() == Elf::PT_GNU_EH_FRAME) {
							result->dlfo_eh_frame = obj->base + s.virt_addr();
							break;
						}
					return 0;
				}
	return -1;
}
#endif

#if GLIBC_VERSION < GLIBC_2_36
static __attribute__((unused)) int _dl_discover_osversion() {
	LOG_ERROR << "GLIBC _dl_discover_osversion not implemented" << endl;
	// should return the kernel version as integer
	return 0;
}
#endif

[[maybe_unused]] static void * resolve(const Loader & loader, const char * name) {
	GuardedReader _{loader.lookup_sync};
	auto sym = loader.resolve_symbol(name);
	return sym ? reinterpret_cast<void*>(sym->object().base + sym->value()) : nullptr;
}

[[maybe_unused]] static void clear_list(GLIBC::RTLD::Global::list_t & l) {
	l.next = &l;
	l.prev = &l;
}

void init_globals(const Loader & loader) {
	uintptr_t sysinfo = 0;
	Auxiliary * auxv = Auxiliary::begin();
	rtld_global_ro._dl_auxv = auxv;
	int seen = 0;
	int uid = 0;
	int gid = 0;

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
#if GLIBC_VERSION >= GLIBC_2_34
			case Auxiliary::AT_MINSIGSTKSZ:
				rtld_global_ro._dl_minsigstacksize = aux.value();
				break;
#endif

			case Auxiliary::AT_UID:
				uid ^= aux.value();
				seen |= 1;
				break;
			case Auxiliary::AT_EUID:
				uid ^= aux.value();
				seen |= 2;
				break;
			case Auxiliary::AT_GID:
				gid ^= aux.value();
				seen |= 4;
				break;
			case Auxiliary::AT_EGID:
				gid ^= aux.value();
				seen |= 8;
				break;
			case Auxiliary::AT_SECURE:
				seen = -1;
				__libc_enable_secure = aux.value();
				break;
		}
	}
	if (seen == 0xf) {
		__libc_enable_secure = uid != 0 || gid != 0;
	}

	(void) sysinfo;

#if  GLIBC_VERSION < GLIBC_2_25
	rtld_global._dl_error_catch_tsd = _dl_error_catch_tsd;
#endif
#if !GLIBC_PTHREAD_IN_LIBC
	rtld_global._dl_rtld_lock_recursive = _dl_rtld_lock_recursive;
	rtld_global._dl_rtld_unlock_recursive = _dl_rtld_unlock_recursive;
#endif
#if GLIBC_VERSION < GLIBC_2_34 || !GLIBC_PTHREAD_IN_LIBC
	rtld_global._dl_make_stack_executable_hook = _dl_make_stack_executable;
	rtld_global._dl_init_static_tls = _dl_init_static_tls;
#endif
#if GLIBC_VERSION < GLIBC_2_33
	rtld_global._dl_wait_lookup_done = _dl_wait_lookup_done;
#endif

#if GLIBC_VERSION >= GLIBC_2_31 && !defined(COMPATIBILITY_DEBIAN_BUSTER)
	rtld_global_ro._dl_vdso_clock_gettime64 = resolve(loader, "__vdso_clock_gettime");
	rtld_global_ro._dl_vdso_gettimeofday = resolve(loader, "__vdso_gettimeofday");
	rtld_global_ro._dl_vdso_time = resolve(loader, "__vdso_time");
	rtld_global_ro._dl_vdso_getcpu = resolve(loader, "__vdso_getcpu");
	rtld_global_ro._dl_vdso_clock_getres_time64 = resolve(loader, "__vdso_clock_getres");
#endif
	rtld_global_ro._dl_debug_printf = _dl_debug_printf;
#if GLIBC_VERSION < GLIBC_2_25
	rtld_global_ro._dl_catch_error = _dl_catch_error;
	rtld_global_ro._dl_signal_error = _dl_signal_error;
#endif
	rtld_global_ro._dl_mcount = _dl_mcount;
	rtld_global_ro._dl_lookup_symbol_x = _dl_lookup_symbol_x;
#if GLIBC_VERSION < GLIBC_2_28
	rtld_global_ro._dl_check_caller = _dl_check_caller;
#endif
	rtld_global_ro._dl_open = _dl_open;
	rtld_global_ro._dl_close = _dl_close;
#if GLIBC_VERSION >= GLIBC_2_34
	rtld_global_ro._dl_catch_error = _dl_catch_error;
	rtld_global_ro._dl_error_free = _dl_error_free;
#endif
	rtld_global_ro._dl_tls_get_addr_soft = ::_dl_tls_get_addr_soft;
#if GLIBC_VERSION >= GLIBC_2_35
	rtld_global_ro._dl_libc_freeres = _dl_libc_freeres;
	rtld_global_ro._dl_find_object = _dl_find_object;
#endif

#if GLIBC_VERSION < GLIBC_2_36
	rtld_global_ro._dl_discover_osversion = _dl_discover_osversion;
#endif

#if GLIBC_PTHREAD_IN_LIBC && GLIBC_VERSION >= GLIBC_2_33
	clear_list(rtld_global._dl_stack_used);
	clear_list(rtld_global._dl_stack_user);
 #if GLIBC_VERSION >= GLIBC_2_34
	clear_list(rtld_global._dl_stack_cache);
 #endif
#endif

	assert(loader.target != nullptr);
	rtld_global._dl_ns[NAMESPACE_BASE]._ns_loaded = &(loader.target->glibc_link_map);
	rtld_global._dl_ns[NAMESPACE_BASE]._ns_nloaded = loader.lookup.size();
	Memory::copy(&rtld_global._dl_rtld_map, loader.self, sizeof(GLIBC::DL::link_map));
}


void init_globals_tls(const TLS & tls, void * dtv) {
	rtld_global._dl_tls_max_dtv_idx = tls.modules.size();
	rtld_global._dl_tls_generation = tls.gen;
	rtld_global._dl_initial_dtv = dtv;
	rtld_global._dl_tls_static_nelem = tls.initial_count;
	rtld_global._dl_tls_static_used = tls.initial_size;
	auto static_size = Math::align_up(tls.initial_size + tls.surplus,  tls.initial_align) + sizeof(Thread);
#if GLIBC_VERSION >= GLIBC_2_34
	rtld_global_ro._dl_tls_static_size = static_size;
	rtld_global_ro._dl_tls_static_align = tls.initial_align;
#else
	rtld_global._dl_tls_static_size = static_size;
	rtld_global._dl_tls_static_align = tls.initial_align;
#endif
#if GLIBC_VERSION >= GLIBC_2_34 || defined(COMPATIBILITY_DEBIAN_BULLSEYE)
	rtld_global_ro._dl_tls_static_surplus = tls.surplus;
#endif

}

void stack_end(void * ptr) {
	libc_stack_end = ptr;
}

}  // namespace RTLD
}  // namespace GLIBC
