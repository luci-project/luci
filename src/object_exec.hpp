#pragma once

#include "object.hpp"

struct ObjectExecutable : public Object {
	ObjectExecutable(const Object::File & file = Object::File()) : Object(file) {}

 protected:
	virtual bool preload() override {
		return preload_segments();
	}

	/*! \brief initialize segments */
	bool preload_segments();

};
