#pragma once

#include "object.hpp"

struct ObjectRelocatable : Object {
	using Object::Object;

 protected:
	bool load() {
		return false;
	};
};
