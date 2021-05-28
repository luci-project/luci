#pragma once

#include "object/base.hpp"

struct ObjectRelocatable : public Object {
	ObjectRelocatable(ObjectIdentity & file, const Object::Data & data) : Object{file, data} {}

	/*! \brief Relocate sections */
	bool prepare() override;

 protected:
	bool preload() override {
		return false;
	};
};
