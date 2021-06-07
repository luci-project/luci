#include <dlh/alloc.hpp>
#include <dlh/assert.hpp>
#include <dlh/string.hpp>
#include <dlh/unistd.hpp>
#include <dlh/math.hpp>
#include <dlh/mutex.hpp>

#include "internal/alloc_buddy.hpp"


#ifndef ALLOC_LOG2
// Default: 2^31 = 2GB
#define ALLOC_LOG2 31
#endif

static Allocator::Buddy<4, ALLOC_LOG2> allocator;

static Mutex mutex;

void * malloc(size_t size) {
	if (size == 0) {
		return nullptr;
	} else {
		mutex.lock();
		uintptr_t ptr = allocator.malloc(size);
		mutex.unlock();
		return reinterpret_cast<void*>(ptr);
	}
}

void free(void *ptr) {
	if (ptr != nullptr) {
		mutex.lock();
		allocator.free(reinterpret_cast<uintptr_t>(ptr));
		mutex.unlock();
	}
}

void *realloc(void *ptr, size_t size) {
	if (ptr == nullptr && size == 0)
		return nullptr;

	void *new_ptr = nullptr;

	mutex.lock();
	// Allocate (of size > 0)
	if (size > 0 && (new_ptr = reinterpret_cast<void*>(allocator.malloc(size))) != nullptr && ptr != nullptr) {
		// Copy contents
		memcpy(new_ptr, ptr, Math::min(size, allocator.size(reinterpret_cast<uintptr_t>(ptr))));
	}

	// Free old allocation
	if (ptr != nullptr)
		allocator.free(reinterpret_cast<uintptr_t>(ptr));

	mutex.unlock();
	return new_ptr;
}

void * calloc(size_t nmemb, size_t size) {
	// Catch integer-multiplication overflow
	size_t bytes = nmemb * size;
	if (size == 0 || bytes / size != nmemb) {
		return nullptr;
	}

	// Initialize memory
	void *new_ptr = malloc(bytes);
	if (new_ptr != nullptr) {
		memset(new_ptr, 0, bytes);
	}
	return new_ptr;
}
