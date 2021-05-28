#include "object/base.hpp"

#include "libc/errno.hpp"
#include "utils/auxiliary.hpp"
#include "utils/log.hpp"

#include "object/dynamic.hpp"
#include "object/executable.hpp"
#include "object/relocatable.hpp"

#include "loader.hpp"


Object::Object(ObjectIdentity & file, const Data & data) : Elf(reinterpret_cast<uintptr_t>(data.ptr)), file(file), data(data) {
	assert(data.ptr != nullptr);
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

	errno = 0;
	if (munmap(data.ptr, data.size) == -1) {
		LOG_ERROR << "Unmapping data from " << *this << " failed: " << strerror(errno) << endl;
	}

	// close file descriptor
	if (data.fd > 0)
		close(data.fd);

	// TODO: unmap file.data?
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
