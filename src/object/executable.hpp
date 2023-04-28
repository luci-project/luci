// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include "object/identity.hpp"
#include "object/base.hpp"

struct ObjectExecutable : public Object {
	ObjectExecutable(ObjectIdentity & file, const Object::Data & data) : Object{file, data} {}

 protected:
	bool preload() override {
		return preload_segments(false);
	}

	/*! \brief initialize segments */
	bool preload_segments(bool setup_relro);
};
