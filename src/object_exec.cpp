#include "object_exec.hpp"

#include "generic.hpp"

bool ObjectExecutable::preload() {
	return preload_segments();
}

bool ObjectExecutable::preload_segments(uintptr_t base) {
	// load segments
	for (auto & segment : elf.segments)
		if (Elf::PT_LOAD == segment.type())
			memory_map.emplace_back(*this, segment, base);
	return memory_map.size() > 0;
}
