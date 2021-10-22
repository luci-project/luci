#include "compatibility/glibc/libdl/interface.hpp"

#include <dlh/log.hpp>
#include <dlh/macro.hpp>

#include "compatibility/gdb.hpp"
#include "object/base.hpp"
#include "loader.hpp"

const void* RTLD_NEXT = reinterpret_cast<void*>(-1l);
const void* RTLD_DEFAULT = 0;

// TODO: Should be __thread
static const char * error_msg = nullptr;

EXPORT int dlclose(void *) {
	error_msg = "Unloading is not supported (yet)";
	LOG_WARNING << "dlclose failed: " << error_msg << endl;
	return -1;
}

EXPORT const char *dlerror() {
	const char * msg = error_msg;
	error_msg = nullptr;
	return msg;
}


EXPORT void *dlopen(const char * filename, int flags) {
	return dlmopen(GLIBC::DL::LM_ID_BASE, filename, flags);
}

EXPORT void *dlmopen(GLIBC::DL::Lmid_t lmid, const char *filename, int flags) {
	LOG_TRACE << "GLIBC dlmopen(" << (int)lmid << ", " << filename << ", "  << flags << ")" << endl;
	auto loader = Loader::instance();
	assert(loader != nullptr);

	ObjectIdentity::Flags objflags;
	objflags.bind_now = (flags & GLIBC::DL::RTLD_NOW) != 0;
	objflags.bind_global = (flags & GLIBC::DL::RTLD_GLOBAL) != 0;
	objflags.persistent = (flags & GLIBC::DL::RTLD_NODELETE) != 0;
	objflags.bind_deep = (flags & GLIBC::DL::RTLD_DEEPBIND) != 0;

	auto r = loader->dlopen(filename, objflags, lmid, (flags & GLIBC::DL::RTLD_NOLOAD) == 0);
	if (r == nullptr) {
		error_msg = "Unable to open shared object!";
		LOG_WARNING << "dl[m]open of " << filename << " failed: " << error_msg << endl;
	}
	return reinterpret_cast<void*>(r);
}

enum : int {
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

EXPORT int dlinfo(void * __restrict handle, int request, void * __restrict info) {
	LOG_TRACE << "GLIBC dlinfo(" << handle << ", "  << request << ", " << info << ")" << endl;
	auto loader = Loader::instance();
	assert(loader != nullptr);

	auto o = reinterpret_cast<const ObjectIdentity *>(handle);
	if (!loader->is_loaded(o)) {
		error_msg = "Invalid handle for dlinfo!";
		LOG_WARNING << error_msg << endl;
		return -1;
	}

	switch(request) {
		case RTLD_DI_LMID:
			*((GLIBC::DL::Lmid_t*)info) = o->ns;
			return 0;
		case RTLD_DI_LINKMAP:
			*((const ObjectIdentity**)info) = o;
			return 0;
		case RTLD_DI_SERINFO:
			error_msg = "Request RTLD_DI_SERINFO not implemented (yet)!";
			LOG_WARNING << "dlinfo failed: " << error_msg << endl;
			return -1;
		case RTLD_DI_SERINFOSIZE:
			error_msg = "Request RTLD_DI_SERINFOSIZE not implemented (yet)!";
			LOG_WARNING << "dlinfo failed: " << error_msg << endl;
			return -1;
		case RTLD_DI_ORIGIN:
			*((const char**)info) = o->path.str;
			return 0;
		case RTLD_DI_TLS_MODID:
			*((size_t*)info) = o->tls_module_id;
			return 0;
		case RTLD_DI_TLS_DATA:
			*((uintptr_t*)info) = o->tls_offset == 0 ? 0 : loader->tls.get_addr(Thread::self(), o->tls_module_id, false);
			return 0;
		default:
			error_msg = "Unknown request for dlinfo!";
			LOG_WARNING << error_msg << endl;
			return -1;
	}
}


EXPORT int dladdr1(void *addr, GLIBC::DL::Info *info, void **extra_info, int flags) {
	LOG_TRACE << "GLIBC dladdr(" << addr << ", " << (void*)info << ", " << (void*)extra_info << ", " << flags << ")" << endl;
	auto loader = Loader::instance();
	assert(loader != nullptr);

	auto o = loader->resolve_object(reinterpret_cast<uintptr_t>(addr));
	if (o == nullptr)
		return 0;

	assert(info != nullptr);
	info->dli_fname = o->file.filename;
	info->dli_fbase = o->base;
	if (flags == GLIBC::DL::RTLD_DL_LINKMAP)
		*extra_info = reinterpret_cast<void*>(&(o->file));
	auto sym = o->resolve_symbol(reinterpret_cast<uintptr_t>(addr));
	if (sym) {
		info->dli_sname = sym->name();
		info->dli_saddr = o->base + sym->value();
		if (flags == GLIBC::DL::RTLD_DL_SYMENT)
			*extra_info = (void *)(sym->_data);
	} else {
		info->dli_sname = nullptr;
		info->dli_saddr = 0;
	}

	return 1;
}

EXPORT int dladdr(void *addr, GLIBC::DL::Info *info) {
	LOG_TRACE << "GLIBC dladdr( " << addr << " ," << (void*)info << ")" << endl;
	return dladdr1(addr, info, 0, 0);
}

static void *_dlvsym(void *__restrict handle, const char *__restrict symbol, const char *__restrict version, void * caller) {
	auto loader = Loader::instance();
	assert(loader != nullptr);

	Optional<VersionedSymbol> r;
	if (handle == RTLD_DEFAULT || handle == RTLD_NEXT) {
		auto o = loader->resolve_object(reinterpret_cast<uintptr_t>(caller));
		assert(o != nullptr);
		r = loader->resolve_symbol(symbol, version, o->file.ns, &(o->file), handle == RTLD_NEXT ? Loader::RESOLVE_AFTER_OBJECT : Loader::RESOLVE_DEFAULT);
	} else {
		auto o = reinterpret_cast<const ObjectIdentity *>(handle);
		if (!loader->is_loaded(o)) {
			error_msg = "Invalid handle for dl[v]sym!";
			LOG_WARNING << error_msg << endl;
			return nullptr;
		}

		r = o->current->resolve_symbol(symbol, version);
	}

	if (r && r->valid()) {
		return reinterpret_cast<void*>(r->object().base + r->value());
	} else {
		error_msg = "Symbol not found!";
		LOG_WARNING << "dl[v]sym for " << symbol << " failed: " << error_msg << endl;
		return nullptr;
	}
}


EXPORT void *dlsym(void *__restrict handle, const char *__restrict symbol) {
	LOG_TRACE << "GLIBC dlsym( " << handle << ", " << symbol << ")" << endl;
	return _dlvsym(handle, symbol, nullptr, __builtin_extract_return_addr(__builtin_return_address(0)));
}

EXPORT void *dlvsym(void *__restrict handle, const char *__restrict symbol, const char *__restrict version) {
	LOG_TRACE << "GLIBC dlvsym( " << handle << ", " << symbol << ", " << version << ")" << endl;
	return _dlvsym(handle, symbol, version, __builtin_extract_return_addr(__builtin_return_address(0)));
}