// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <dlh/log.hpp>

void *dso_handle = &dso_handle;
extern __attribute__((alias("dso_handle"), visibility("default"))) void * __dso_handle;
