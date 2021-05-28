#pragma once

#include <vector>

namespace File {

bool exists(const char * path);

bool readable(const char * path);

bool writeable(const char * path);

bool executable(const char * path);

std::vector<const char *> contents(const char * path);

}  // Namespace File
