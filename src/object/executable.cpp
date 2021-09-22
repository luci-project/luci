#include "object/executable.hpp"

#include "loader.hpp"

bool ObjectExecutable::preload_segments() {
	size_t load = 0;
	for (auto & segment : this->segments)
		if (Elf::PT_LOAD == segment.type() && segment.virt_size() > 0)
			memory_map.emplace_back(*this, segment, base);
		else if (Elf::PT_TLS == segment.type() && segment.virt_size() > 0 && this->file.tls_module_id == 0)
			this->file.tls_module_id = this->file.loader.tls.add_module(this->file, segment.virt_size(), segment.alignment(), segment.virt_addr(), segment.size(), this->file.tls_offset);

	// Shared data on updates
	if (file_previous != nullptr) {
		auto & prev_memory_map = file_previous->memory_map;
		// Check mappings
		if (memory_map.size() != prev_memory_map.size()) {
			LOG_WARNING << "Load segements differ in " << file << " (" << this->memory_map.size() << " compared to " << prev_memory_map.size() <<  " in the current version) - unable to handle this yet" << endl;
			return false;
		} else {
			// Copy Memory FD (for shared data)
			for (size_t m = 0; m < memory_map.size(); m++)
				if (prev_memory_map[m].target.fd != -1)
					memory_map[m].target.fd = prev_memory_map[m].target.fd;
		}
	}

	return memory_map.size() > 0;
}
