#pragma once

#include "object.hpp"

struct ObjectExecutable : Object {
	using Object::Object;

 protected:
	bool load();

	/*! \brief initialize segments */
	bool load_segments(uintptr_t base = 0);
};
