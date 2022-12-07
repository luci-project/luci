#include "object/base.hpp"

#include <dlh/syscall.hpp>
#include <dlh/auxiliary.hpp>
#include <dlh/utility.hpp>
#include <dlh/log.hpp>

#include "object/identity.hpp"
#include "object/dynamic.hpp"
#include "object/executable.hpp"
#include "object/relocatable.hpp"

#include "loader.hpp"


Object::Object(ObjectIdentity & file, const Data & data) : Elf(data.addr), file(file), data(data) {
	assert(data.addr != 0);
	// Build ID
	build_id[0] = '\0';
	if (file.flags.updatable)
		for (auto & section: this->sections)
			if (section.type() == Elf::SHT_NOTE)
				for (auto & note : section.get_notes())
					if (note.name() != nullptr && strcmp(note.name(), "GNU") == 0 && note.type() == Elf::NT_GNU_BUILD_ID) {
						BufferStream id(build_id, count(this->build_id));
						auto desc = reinterpret_cast<const uint8_t *>(note.description());

						for (size_t i = 0; i < note.size(); i++)
							id << hex << right << setfill('0') << setw(2)  << static_cast<uint32_t>(desc[i]);
						id.flush();
						assert(build_id[count(build_id) - 1] == '\0');
						break;
					}
}

Object::~Object() {
	// TODO: Not really supported yet, just a stub...

	if (debug_hash != nullptr)
		Memory::free(debug_hash);

	// Remove this version from list
	if (file.current == this)
		file.current = file_previous;
	else
		for (auto tmp = file.current; tmp != nullptr; tmp = tmp->file_previous)
			if (tmp->file_previous == this) {
				tmp->file_previous = file_previous;
				break;
			}

	// Unmap virt mem
	for (auto & seg : memory_map)
		seg.unmap();

	if (auto unmap = Syscall::munmap(data.addr, data.size); unmap.failed()) {
		LOG_ERROR << "Unmapping data from " << *this << " failed: " << unmap.error_message() << endl;
	}

	// close file descriptor
	if (data.fd > 0)
		Syscall::close(data.fd);

	// TODO: unmap file.data?
}

const char * Object::query_debug_hash() {
	// Query for DWARF hash, if corresponding socket is connected)
	if (debug_hash == nullptr) {
		auto & socket = file.loader.debug_hash_socket;
		if (socket.is_connected()) {
			char tmp[128];

			// First query for Build ID
			if (build_id[0] != '\0') {
				LOG_INFO << "Quering for debug hash by build id: " << build_id << endl;
				size_t sent = socket.send(build_id, count(build_id));
				if (sent != count(build_id)) {
					LOG_WARNING << "Debug hash socket sent " << sent << " (instead of " << sizeof(build_id) << ") bytes" << endl;
				} else {
					size_t recv = socket.recv(tmp, count(tmp), true);
					LOG_DEBUG << "Debug hash for build id " << build_id << " is " << tmp << endl;
					if (recv > 0 && tmp[0] != '-' && tmp[1] != '\0') {
						return debug_hash = String::duplicate(tmp, recv);
					}
				}
			}

			// Then try path
			if (!file.path.empty()) {
				LOG_INFO << "Quering for debug hash by path: " << file.path << endl;
				size_t sent = socket.send(file.path.str, file.path.len + 1);
				if (sent != file.path.len + 1) {
					LOG_WARNING << "Debug hash socket sent " << sent << " (instead of " << file.path.len << ") bytes" << endl;
				} else {
					size_t recv = socket.recv(tmp, count(tmp), true);
					LOG_DEBUG << "Debug hash for path " << file.path  << " is " << tmp << endl;
					if (recv > 0 && tmp[0] != '-' && tmp[1] != '\0')
						return debug_hash = String::duplicate(tmp, recv);
				}
			}
		}
	}
	return debug_hash;
}

uintptr_t Object::dynamic_address() const {
	for (const auto & segment : this->segments)
		if (segment.type() == Elf::PT_DYNAMIC)
			return (this->header.type() == Elf::ET_EXEC ? 0 : base) + segment.virt_addr();
	return 0;
}

bool Object::is_latest_version() const {
	return this == file.current;
}

bool Object::memory_range(uintptr_t & start, uintptr_t & end) const {
	if (memory_map.size() > 0) {
		start = memory_map.front().target.page_start();
		end = memory_map.back().target.page_end();
		return true;
	} else {
		return false;
	}
}

bool Object::map() {
	bool success = true;
	for (auto & seg : memory_map)
		success &= seg.map();
	return success;
}

bool Object::protect() {
	bool success = true;
	for (auto & seg : memory_map)
		success &= seg.protect();
	return success;
}

bool Object::unprotect() {
	bool success = true;
	for (auto & seg : memory_map)
		success &= seg.unprotect();
	return success;
}

bool Object::disable() {
	bool success = true;
	for (MemorySegment &mem: memory_map)
		if ((mem.target.protection & PROT_EXEC) != 0)
			success &= mem.disable();
	return success;
}

size_t Object::version() const {
	size_t v = 0;
	for (Object * p = file_previous; p != nullptr; p = p->file_previous)
		v++;
	return v;
}

void* Object::dynamic_resolve(size_t index) const {
	LOG_ERROR << "Unable to resolve " << index << " -- Object " << file.path << " does not support dynamic loading!" << endl;
	assert(false);
	return nullptr;
}

bool Object::has_symbol(const char * name, uint32_t hash, uint32_t gnu_hash, const VersionedSymbol::Version & version, Optional<VersionedSymbol> & result) const {
	auto tmp = resolve_symbol(name, hash, gnu_hash, version);
	if (tmp) {
		assert(tmp->valid());
		assert(tmp->bind() != Elf::STB_LOCAL); // should not be returned
		// Weak dynamic linkage is only taken into account, if file.loader.config.dynamic_weak is set. Otherwise it is always strong.
		bool strong = tmp->bind() != Elf::STB_WEAK || !file.loader.config.dynamic_weak;
		if (strong || !result) {
			result = tmp;
			return strong;
		}
	}
	return false;
}
