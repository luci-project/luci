#include "tls.hpp"

#include <dlh/errno.hpp>
#include <dlh/unistd.hpp>
#include <dlh/utils/log.hpp>
#include <dlh/utils/math.hpp>
#include <dlh/utils/thread.hpp>
#include <dlh/container/vector.hpp>

/*! \brief Addtional space reserved for DTV (negative offset) */
static const size_t dtv_magic_offset = 1;

void TLS::dtv_setup(Thread * thread) {
	if (gen == 0)
		gen = 1;

	// Allocate DTV with enough space
	if (thread->dtv == nullptr && dtv_allocate(thread) == 0) {
		LOG_ERROR << "Allocating DTV for Thread " << reinterpret_cast<void*>(thread->tcb)<< " failed" << endl;
		assert(false);
	}

	// Initialize array
	assert(thread->tcb == thread);
	uintptr_t start = reinterpret_cast<uintptr_t>(thread->tcb);
	assert(start >= initial_size);
	assert(dtv_module_size(thread->dtv) >= initial_count);
	for (size_t mid = 1; mid <= initial_count; mid++) {
		assert(thread->dtv[mid].pointer.val == TLS_UNALLOCATED);
		auto & module = modules[mid - 1];

		// Calculate address of module
		auto addr = start - module.offset;
		assert(addr % module.align == 0);
		assert(addr < start);  // only x86_64

		// Copy data & assign pointer
		dtv_copy(thread, mid, reinterpret_cast<void*>(addr));
	}
	assert(start % initial_align == 0);

	// Set generation
	dtv_generation(thread->dtv) = gen;
}

size_t TLS::dtv_allocate(Thread * thread) {
	size_t new_size = Math::max(modules.size() * 2, 16U);
	size_t n = new_size + dtv_magic_offset + 1;
	void * ptr;
	if (thread->dtv == nullptr)
		ptr = calloc(n, sizeof(Thread::DynamicThreadVector));
	else
		ptr = realloc(reinterpret_cast<void*>(thread->dtv - dtv_magic_offset), n * sizeof(Thread::DynamicThreadVector));
	if (ptr == nullptr)
		return 0;

	thread->dtv = reinterpret_cast<Thread::DynamicThreadVector*>(ptr) + dtv_magic_offset;

	size_t &dtv_size = dtv_module_size(thread->dtv);
	// Set new dynamic modules to TLS_UNALLOCATED
	for (size_t i = dtv_size + 1; i <= new_size; i++) {
		thread->dtv[i].pointer.val = TLS_UNALLOCATED;
		thread->dtv[i].pointer.to_free = nullptr;
	}
	dtv_size = new_size;
	return new_size;
}

void TLS::dtv_free(Thread * thread) {
	auto count = dtv_module_size(thread->dtv);
	assert(count >= initial_count);
	for (size_t i = initial_count; i < count; i++)
		if (thread->dtv[i].pointer.val != TLS_UNALLOCATED)
			::free(*(reinterpret_cast<void**>(thread->dtv[i].pointer.val) - 1));

	::free(thread->dtv - dtv_magic_offset);
	thread->dtv = nullptr;
}

void TLS::dtv_copy(Thread * thread, size_t module_id, void * ptr) const {
	auto & module = modules[module_id - 1];
	auto & dtv_ptr = thread->dtv[module_id].pointer;
	assert(dtv_ptr.val == TLS_UNALLOCATED);

	// Copy tdata
	LOG_INFO << "Copy " << module.image_size << " from " << reinterpret_cast<void*>(module.object.current->base + module.image) << " to " << ptr << endl;
	memcpy(ptr, reinterpret_cast<void*>(module.object.current->base + module.image), module.image_size);
	// Clear tbss
	if (module.size > module.image_size)
		memset(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) + module.image_size), 0, module.size - module.image_size);

	// Assign pointer DTV structure
	dtv_ptr.val = ptr;
	assert(dtv_ptr.to_free == nullptr);  // or glibc will try to free it (but we didn't use its allocator)...
}

Thread * TLS::allocate(Thread * thread, bool set_fs) {
	if (thread == nullptr) {
		void * mem = calloc(initial_size + TLS_THREAD_SIZE + initial_align, 1);
		assert(mem != nullptr);

		uintptr_t addr = Math::align(reinterpret_cast<uintptr_t>(mem), initial_align);
		assert(TLS_THREAD_SIZE >= sizeof(Thread));
		thread = new (reinterpret_cast<Thread*>(addr + initial_size)) Thread(nullptr, mem, initial_size + TLS_THREAD_SIZE);
	}

	dtv_allocate(thread);

	if (set_fs) {
		if (arch_prctl(ARCH_SET_FS, reinterpret_cast<uintptr_t>(thread)) == 0) {
			LOG_INFO << "Changed %fs of " << ::gettid() << " to " << (void*)thread << endl;
		} else {
			LOG_WARNING << "Changing %fs of " << ::gettid() << " to " << (void*)thread << " failed: " << __errno_string(errno) << endl;
		}
	}

	return thread;
}

void TLS::free(Thread * thread, bool free_thread_struct) {
	assert(thread != nullptr);

	dtv_free(thread);
	if (free_thread_struct) {
		assert(thread->map_size == initial_size + TLS_THREAD_SIZE);
		::free(thread->map_base);
	}
}
