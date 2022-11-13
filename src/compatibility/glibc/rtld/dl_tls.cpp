#include "compatibility/glibc/rtld/dl_tls.hpp"

#include <dlh/log.hpp>
#include <dlh/macro.hpp>
#include <dlh/assert.hpp>
#include <dlh/thread.hpp>

#include "loader.hpp"

EXPORT uintptr_t __tls_get_addr(struct tls_index *ti) {
	if (ti == nullptr)
		return 0;

	auto thread = Thread::self();
	assert(thread != nullptr);

	auto loader = Loader::instance();
	assert(loader != nullptr);

	return loader->tls.get_addr(thread, ti->ti_module) + ti->ti_offset;
}

EXPORT void _dl_get_tls_static_info(size_t *size, size_t *align) {
	LOG_TRACE << "GLIBC _dl_get_tls_static_info()" << endl;
	auto loader = Loader::instance();
	assert(loader != nullptr);

	*size = loader->tls.initial_size + sizeof(Thread);
	*align = loader->tls.initial_align;
}

// This function is called from within pthread_create to initialize
// the content of the dtv for a new thread before giving control to
// that new thread
EXPORT Thread * _dl_allocate_tls_init(Thread *thread) {
	LOG_TRACE << "GLIBC _dl_allocate_tls_init(" << (void*)thread << ")" << endl;
	if (thread == nullptr)
		return nullptr;

	auto loader = Loader::instance();
	assert(loader != nullptr);

	loader->tls.dtv_setup(thread);

	return thread;
}

EXPORT Thread * _dl_allocate_tls(Thread * thread) {
	LOG_TRACE << "GLIBC _dl_allocate_tls(" << (void*)thread << ")" << endl;
	auto loader = Loader::instance();
	assert(loader != nullptr);

	thread = loader->tls.allocate(thread);
	loader->tls.dtv_setup(thread);
	return thread;
}

EXPORT void _dl_deallocate_tls(Thread * thread, bool free_thread_struct) {
	LOG_TRACE << "GLIBC _dl_deallocate_tls(" << (void*)thread << ")" << endl;
	auto loader = Loader::instance();
	assert(loader != nullptr);

	loader->tls.free(thread, free_thread_struct);
}

void * _dl_tls_get_addr_soft(GLIBC::DL::link_map *l) {
	(void) l;
	LOG_ERROR << "GLIBC _dl_tls_get_addr_soft not implemented" << endl;
	return nullptr;
}
