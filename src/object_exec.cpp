#include "object_exec.hpp"

bool ObjectExecutable::load() {
	return load_segments();
}


bool ObjectExecutable::load_segments(uintptr_t base) {
	// load segments
	auto seg_size = elf.segments.size();
	for (ELFIO::Elf_Half i = 0; i < seg_size; ++i ) {
		ELFIO::segment* seg = elf.segments[i];
		if (PT_LOAD == seg->get_type()) {
			Segment new_segment;
			new_segment.file.offset = seg->get_offset();
			new_segment.file.size = seg->get_file_size();
			new_segment.memory.base = base;
			new_segment.memory.offset = seg->get_virtual_address();
			new_segment.memory.size = seg->get_memory_size();
			new_segment.protection = 0;
			if (seg->get_flags() & PF_R) {
				new_segment.protection |= PROT_READ;
			}
			if (seg->get_flags() & PF_W) {
				new_segment.protection |= PROT_WRITE;
			}
			if (seg->get_flags() & PF_X) {
				new_segment.protection |= PROT_EXEC;
			}
			segments.push_back(new_segment);
		}
	}
	return segments.size() > 0;
}
