#include <dlh/types.hpp>
#include <dlh/assert.hpp>
#include <dlh/utils/log.hpp>
#include <dlh/utils/thread.hpp>

#include "compatibility/export.hpp"

#include "loader.hpp"

struct tls_index {
	size_t ti_module;
	uintptr_t ti_offset;
};

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
	auto loader = Loader::instance();
	assert(loader != nullptr);

	assert(TLS_THREAD_SIZE >= sizeof(Thread));
	*size = loader->tls.initial_size + TLS_THREAD_SIZE;
	*align = loader->tls.initial_align;
}

// This function is called from within pthread_create to initialize
// the content of the dtv for a new thread before giving control to
// that new thread
EXPORT Thread * _dl_allocate_tls_init(Thread *thread) {
	if (thread == nullptr)
		return nullptr;

	auto loader = Loader::instance();
	assert(loader != nullptr);

	loader->tls.dtv_setup(thread);

	return thread;
}

EXPORT Thread * _dl_allocate_tls(Thread * thread) {
	auto loader = Loader::instance();
	assert(loader != nullptr);

	return loader->tls.allocate(thread);
}

EXPORT void _dl_deallocate_tls(Thread * thread, bool free_thread_struct) {
	auto loader = Loader::instance();
	assert(loader != nullptr);

	loader->tls.free(thread, free_thread_struct);
}
