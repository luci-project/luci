#include "object/executable.hpp"

#include <dlh/assert.hpp>

#include "loader.hpp"
#include "page.hpp"

bool ObjectExecutable::preload_segments() {
	size_t load = 0;

	// Check relocation read-only
	const Elf::Segment * relro = nullptr;
	for (auto & segment : this->segments)
		if (Elf::PT_GNU_RELRO == segment.type()) {
			assert((segment.virt_addr() + segment.size()) % Page::SIZE == 0);
			assert(segment.virt_size() == segment.size());
			relro = &segment;
		}

	// LOAD segments
	for (auto & segment : this->segments)
		if (Elf::PT_LOAD == segment.type() && segment.virt_size() > 0) {
			if (relro != nullptr && segment.offset() == relro->offset() && segment.virt_addr() == relro->virt_addr()) {
				LOG_DEBUG << "Relocation read-only at " << reinterpret_cast<void*>(relro->virt_addr()) << " with " << relro->size() << "bytes" << endl;
				// Relro section
				memory_map.emplace_back(*this, *relro, base);
				// Rest of data section
				memory_map.emplace_back(*this, segment, base, relro->size());
			} else {
				memory_map.emplace_back(*this, segment, base);
			}
		}
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


bool ObjectExecutable::unprotect() {
	// make relocation read-only segments writable
	bool success = true;
	for (auto & mem : memory_map)
		if (mem.target.relro)
			success &= mem.unprotect();
	return success;
}
