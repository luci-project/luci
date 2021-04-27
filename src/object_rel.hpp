#pragma once

#include "object.hpp"

struct ObjectRelocatable : public Object {
	ObjectRelocatable(const Object::File & file = Object::File()) : Object(file) {}

	/*! \brief Relocate sections */
	bool run_relocate(bool bind_now = false) override;

 protected:
	bool preload() override {
		return false;
	};
};
