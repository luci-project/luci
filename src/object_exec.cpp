#include "object_exec.hpp"

#include "generic.hpp"

bool ObjectExecutable::preload_segments() {
	// load segments
	for (auto & segment : elf.segments)
		if (Elf::PT_LOAD == segment.type() && segment.virt_size() > 0)
			memory_map.emplace_back(*this, segment, base);
	return memory_map.size() > 0;
}
