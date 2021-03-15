#pragma once

#include "object_exec.hpp"

struct ObjectDynamic : ObjectExecutable {
	using ObjectExecutable::ObjectExecutable;

 protected:
	bool load();

	/*! \brief load required libaries */
	bool load_libraries();
};
