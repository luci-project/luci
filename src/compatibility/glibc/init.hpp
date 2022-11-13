#pragma once

#include "loader.hpp"
#include "object/dynamic.hpp"
#include "object/identity.hpp"

namespace GLIBC {
bool init(Loader & loader);
bool init(const ObjectDynamic & object);
void init_tls(ObjectIdentity & object, const size_t size, const size_t align, const uintptr_t image, const size_t image_size, const intptr_t offset, size_t modid);
}  // namespace GLIBC
