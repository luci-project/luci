#include "object/executable.hpp"

bool ObjectExecutable::preload_segments() {
	// load segments
	for (auto & segment : this->segments)
		if (Elf::PT_LOAD == segment.type() && segment.virt_size() > 0)
			memory_map.emplace_back(*this, segment, base);
	return memory_map.size() > 0;
}
