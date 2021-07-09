#include "object/executable.hpp"

#include "loader.hpp"

bool ObjectExecutable::preload_segments() {
	// load segments
	for (auto & segment : this->segments)
		if (Elf::PT_LOAD == segment.type() && segment.virt_size() > 0)
			memory_map.emplace_back(*this, segment, base);
		else if (Elf::PT_TLS == segment.type() && segment.virt_size() > 0 && this->file.tls_module_id == 0)
			this->file.tls_module_id = this->file.loader.tls.add_module(this->file, segment.virt_size(), segment.alignment(), segment.virt_addr(), segment.size(), this->file.tls_offset);
	return memory_map.size() > 0;
}
