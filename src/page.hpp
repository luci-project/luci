#pragma once

#include <dlh/types.hpp>

// todo: query system for page size

struct Page {
	static const size_t SIZE = 0x1000;
	uintptr_t addr;
	size_t size;

	Page(uintptr_t addr, size_t size) : addr(addr), size(size) {}

	uintptr_t base() {
		return addr - (addr % SIZE);
	}

	size_t length() {
		size_t tmp = ((addr - base()) + size + SIZE - 1);
		return tmp - (tmp % SIZE);
	}

	uintptr_t end() {
		return base() + length();
	}
};
