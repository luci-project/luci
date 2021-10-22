#include "tls.hpp"

#include <dlh/log.hpp>
#include <dlh/math.hpp>
#include <dlh/thread.hpp>
#include <dlh/syscall.hpp>
#include <dlh/container/vector.hpp>

/*! \brief Addtional space reserved for DTV (negative offset) */
static const size_t dtv_magic_offset = 1;

void TLS::dtv_setup(Thread * thread) {
	if (gen == 0)
		gen = 1;

	// Allocate DTV with enough space
	if (thread->dtv == nullptr && dtv_allocate(thread) == 0) {
		LOG_ERROR << "Allocating DTV for Thread " << reinterpret_cast<void*>(thread)<< " failed" << endl;
		assert(false);
	}

	// Initialize array
	uintptr_t start = reinterpret_cast<uintptr_t>(thread);
	assert(start >= initial_size);
	assert(dtv_module_size(thread->dtv) >= initial_count);
	for (size_t mid = 1; mid <= initial_count; mid++) {
		assert(!thread->dtv[mid].allocated());
		auto & module = modules[mid - 1];

		// Calculate address of module
		auto addr = start - module.offset;
		assert(addr % module.align == 0);
		assert(addr < start);  // only x86_64

		// Copy data & assign pointer
		dtv_copy(thread, mid, reinterpret_cast<void*>(addr));
	}

	// Set generation
	dtv_generation(thread->dtv) = gen;
}

size_t TLS::dtv_allocate(Thread * thread) {
	size_t new_size = Math::max(modules.size() * 2, 16U);
	size_t n = new_size + dtv_magic_offset + 1;
	void * ptr;
	if (thread->dtv == nullptr)
		ptr = Memory::alloc_array<Thread::DynamicThreadVector>(n);
	else
		ptr = Memory::realloc(reinterpret_cast<void*>(thread->dtv - dtv_magic_offset), n * sizeof(Thread::DynamicThreadVector));
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
		if (!thread->dtv[i].allocated())
			Memory::free(*(reinterpret_cast<uintptr_t*>(thread->dtv[i].pointer.val) - 1));

	Memory::free(thread->dtv - dtv_magic_offset);
	thread->dtv = nullptr;
}

void TLS::dtv_copy(Thread * thread, size_t module_id, void * ptr) const {
	auto & module = modules[module_id - 1];
	assert(!thread->dtv[module_id].allocated());
	auto & dtv_ptr = thread->dtv[module_id].pointer;

	// Copy tdata
	LOG_INFO << "Copy " << module.image_size << " from " << reinterpret_cast<void*>(module.object.current->base + module.image) << " to " << ptr << endl;
	Memory::copy(ptr, reinterpret_cast<void*>(module.object.current->base + module.image), module.image_size);
	// Clear tbss
	if (module.size > module.image_size)
		Memory::set(reinterpret_cast<uintptr_t>(ptr) + module.image_size, 0, module.size - module.image_size);

	// Assign pointer DTV structure
	dtv_ptr.val = ptr;
	assert(dtv_ptr.to_free == nullptr);  // or glibc will try to free it (but we didn't use its allocator)...
}

Thread * TLS::allocate(Thread * thread, bool set_fs) {
	if (thread == nullptr) {
		if (initial_align < 64)
			initial_align = 64;
		uintptr_t mem = Memory::alloc_array(initial_size + TLS_THREAD_SIZE + initial_align, 1);
		assert(mem != 0);

		uintptr_t addr = Math::align_up(mem + initial_size, initial_align);
		assert(TLS_THREAD_SIZE >= sizeof(Thread));
		thread = new (reinterpret_cast<Thread*>(addr)) Thread(nullptr, mem, initial_size + TLS_THREAD_SIZE);
	}

	dtv_allocate(thread);

	if (set_fs) {
		auto set_fs = Syscall::arch_prctl(ARCH_SET_FS, reinterpret_cast<uintptr_t>(thread));
		if (set_fs.success()) {
			LOG_INFO << "Changed %fs of " << Syscall::gettid() << " to " << (void*)thread << endl;
		} else {
			LOG_WARNING << "Changing %fs of " << Syscall::gettid() << " to " << (void*)thread << " failed: " << set_fs.error_message() << endl;
		}
	}

	return thread;
}

void TLS::free(Thread * thread, bool free_thread_struct) {
	assert(thread != nullptr);

	dtv_free(thread);
	if (free_thread_struct) {
		assert(thread->map_size == initial_size + TLS_THREAD_SIZE);
		Memory::free(thread->map_base);
	}
}