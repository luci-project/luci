#pragma once

#include <string>
#include <vector>

#include "auxiliary.hpp"

namespace Utils {

std::vector<const char *> split(char * source, const char delimiter);

std::vector<const char *> file_contents(const char * path);

char * env(const char * name, bool consume = false);

Auxiliary aux(Auxiliary::type type);

}
