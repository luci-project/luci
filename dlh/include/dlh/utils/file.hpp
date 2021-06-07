#pragma once

#include <dlh/container/vector.hpp>

namespace File {

bool exists(const char * path);

bool readable(const char * path);

bool writeable(const char * path);

bool executable(const char * path);

Vector<const char *> contents(const char * path);

}  // Namespace File
