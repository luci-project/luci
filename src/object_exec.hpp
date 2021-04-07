#pragma once

#include "object.hpp"

struct ObjectExecutable : public Object {
	ObjectExecutable() {}

	ObjectExecutable(std::string path, int fd, void * mem, DL::Lmid_t ns)
	  : Object(path, fd, mem, ns) {}

 protected:
	virtual bool preload() override;

	/*! \brief initialize segments */
	bool preload_segments();

};
