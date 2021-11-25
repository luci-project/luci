#include "object/base.hpp"

#include <dlh/syscall.hpp>
#include <dlh/auxiliary.hpp>
#include <dlh/log.hpp>

#include "object/identity.hpp"
#include "object/dynamic.hpp"
#include "object/executable.hpp"
#include "object/relocatable.hpp"

#include "loader.hpp"


Object::Object(ObjectIdentity & file, const Data & data) : Elf(data.addr), file(file), data(data) {
	assert(data.addr != 0);
}

Object::~Object() {
	// TODO: Not really supported yet, just a stub...

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
	for (auto & seg : memory_map)
		if (!seg.map())
			return false;

	return true;
}

bool Object::protect() {
	for (auto & seg : memory_map)
		if (!seg.protect())
			return false;

	return true;
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
		// Weak dynamic linkage is only taken into account, if file.loader.dynamic_weak is set. Otherwise it is always strong.
		bool strong = tmp->bind() != Elf::STB_WEAK || !file.loader.dynamic_weak;
		if (strong || !result) {
			result = tmp;
			return strong;
		}
	}
	return false;
}
