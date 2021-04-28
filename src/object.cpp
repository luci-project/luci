#include "object.hpp"

#include <fstream>
#include <iostream>

#include "auxiliary.hpp"
#include "loader.hpp"
#include "object_dyn.hpp"
#include "object_exec.hpp"
#include "object_rel.hpp"
#include "generic.hpp"


Object::~Object() {
	for (auto & seg : memory_map)
		seg.unmap();

	if (file.fd > 0)
		close(file.fd);

	free(const_cast<char*>(file.path));

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

bool Object::run_allocate() {
	for (auto & seg : memory_map)
		if (!seg.map())
			return false;

	return true;
}


bool Object::run_protect() {
	for (auto & seg : memory_map)
		if (!seg.protect())
			return false;

	return true;
}

void* Object::dynamic_resolve(size_t index) const {
	LOG_ERROR << "Unable to resolve " << index << " -- Object " << file.path << " does not support dynamic loading!";
	assert(false);
	return nullptr;
}
