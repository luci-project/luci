// Luci - a dynamic linker/loader with DSU capabilities
// Copyright 2021-2023 by Bernhard Heinloth <heinloth@cs.fau.de>
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <dlh/thread.hpp>
#include "object/identity.hpp"


namespace GLIBC {
namespace RTLD {
void starting();
}  // namespace RTLD
}  // nammespace GLIBC


extern "C" ObjectIdentity *_dl_find_dso_for_object(uintptr_t addr);
extern "C" int _dl_make_stack_executable(__attribute__((unused)) void **stack_endp);
extern "C" void _dl_mcount(__attribute__((unused)) uintptr_t from, __attribute__((unused)) uintptr_t to);
extern "C" void __rtld_version_placeholder();
extern "C" int __nptl_change_stack_perm(Thread *t);
extern "C" void __libdl_freeres();
