#pragma once

#include "object.hpp"

struct ObjectRelocatable : public Object {
	ObjectRelocatable(ObjectFile & file, const Object::Data & data) : Object{file, data} {}

	/*! \brief Relocate sections */
	bool prepare() override;

 protected:
	bool preload() override {
		return false;
	};
};
