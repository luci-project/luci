#include <dlh/mem.hpp>
#include <dlh/macro.hpp>
/*
EXPORT_WEAK void * malloc(size_t size) {
	return Memory::alloc<void*>(size);
}

EXPORT_WEAK void free(void *ptr) {
	Memory::free(ptr);
}

EXPORT_WEAK void *realloc(void *ptr, size_t size) {
	return Memory::realloc(ptr, size);
}

EXPORT_WEAK void * calloc(size_t nmemb, size_t size) {
	return reinterpret_cast<void*>(Memory::alloc_array(nmemb, size));
}
*/
