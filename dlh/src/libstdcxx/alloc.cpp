#include <dlh/alloc.hpp>
#include <dlh/assert.hpp>

void *operator new(size_t n) {
	void * ptr = malloc(n);
	assert(ptr != nullptr);
	return ptr;
}

void *operator new[](size_t n) {
	void * ptr = malloc(n);
	assert(ptr != nullptr);
	return ptr;
}

void operator delete[](void *ptr) {
	free(ptr);
}

void operator delete[](void *ptr, size_t size) {
	(void) size;
	free(ptr);
}

void operator delete(void *ptr) {
	free(ptr);
}

void operator delete(void *ptr, size_t size) {
	free(ptr);
}
