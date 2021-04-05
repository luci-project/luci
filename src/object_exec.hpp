#pragma once

#include "object.hpp"

struct ObjectExecutable : public virtual Object {
	ObjectExecutable() {}

	ObjectExecutable(std::string path, int fd, void * mem)
	  : Object(path, fd, mem) {}

 protected:
	virtual bool preload() override;

	/*! \brief initialize segments */
	bool preload_segments(uintptr_t base = 0);

};
