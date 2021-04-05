#pragma once

#include "object.hpp"

struct ObjectRelocatable : public virtual Object {
	ObjectRelocatable() {}

	ObjectRelocatable(std::string path, int fd, void * mem)
	  : Object(path, fd, mem) {}

	/*! \brief Relocate sections */
	bool relocate();

 protected:
	bool preload() {
		return false;
	};
};
