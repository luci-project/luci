#pragma once

#include "object.hpp"

struct ObjectRelocatable : public Object {
	ObjectRelocatable() {}

	ObjectRelocatable(std::string path, int fd, void * mem, DL::Lmid_t ns)
	  : Object(path, fd, mem, ns) {}

	/*! \brief Relocate sections */
	bool relocate();

 protected:
	virtual bool preload() override {
		return false;
	};
};
