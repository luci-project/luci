#include "page.hpp"

#include <dlh/mem.hpp>
#include <dlh/syscall.hpp>

uintptr_t Page::dup() const {
	if (auto mmap = Syscall::mmap(NULL, length(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) {
		return Memory::copy(mmap.value() + (addr % SIZE), addr, size);
	}
	return 0;
}
