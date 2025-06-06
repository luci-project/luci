// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "object/executable.hpp"

#include <dlh/assert.hpp>
#include <dlh/page.hpp>

#include "loader.hpp"


bool ObjectExecutable::preload_segments(bool setup_relro) {
	size_t load = 0;

	// Check relocation read-only
	Optional<Elf::Segment> relro;
	if (setup_relro)
		for (const auto & segment : this->segments)
			if (Elf::PT_GNU_RELRO == segment.type()) {
				// TODO: Check how to handle relro with different size
				if (segment.virt_size() == segment.size()) {
					relro.emplace(segment);
				} else {
					LOG_DEBUG << "Ignore relocation read-only since virtual size of " << segment.virt_size() << " bytes differs from memory size with " << segment.size() << " bytes" << endl;
				}
			}

	// LOAD segments
	for (const auto & segment : this->segments)
		switch (segment.type()) {
			case Elf::PT_LOAD:
				if (segment.virt_size() > 0) {
					// TODO: Support relro for segments other than 4k alignment
					if (relro && segment.offset() == relro->offset() && segment.virt_addr() == relro->virt_addr() && segment.alignment() == 0x1000) {
						LOG_DEBUG << "Relocation read-only at " << reinterpret_cast<void*>(relro->virt_addr()) << " with " << relro->size() << " bytes" << endl;
						// Relro section
						memory_map.emplace_back(*this, *relro, base);
						// Rest of data section (if any)
						if (segment.virt_size() - relro->virt_size() > 0) {
							assert((relro->virt_addr() + relro->virt_size()) % Page::SIZE == 0);
							memory_map.emplace_back(*this, segment, base, relro->virt_size());
						}
					} else {
						memory_map.emplace_back(*this, segment, base);
					}
				}
				break;

			case Elf::PT_TLS:
				if (segment.virt_size() > 0 && this->file.tls_module_id == 0)
					this->file.tls_module_id = this->file.loader.tls.add_module(this->file, segment.virt_size(), segment.alignment(), segment.virt_addr(), segment.size(), this->file.tls_offset);
				break;

			case Elf::PT_GNU_EH_FRAME:
				eh_frame = base + segment.virt_addr();
				break;
		}

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

	return !memory_map.empty();
}
