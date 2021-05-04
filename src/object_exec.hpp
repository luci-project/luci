#pragma once

#include "object.hpp"

struct ObjectExecutable : public Object {
	ObjectExecutable(ObjectFile & file, const Object::Data & data) : Object{file, data} {}


 protected:
	virtual bool preload() override {
		return preload_segments();
	}

	/*! \brief initialize segments */
	bool preload_segments();

};
