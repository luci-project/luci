#pragma once

#include "object/base.hpp"

struct ObjectExecutable : public Object {
	ObjectExecutable(ObjectIdentity & file, const Object::Data & data) : Object{file, data} {}


 protected:
	virtual bool preload() override {
		return preload_segments();
	}

	/*! \brief initialize segments */
	bool preload_segments();

};
