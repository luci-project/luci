#pragma once

#include <string>
#include <vector>

namespace Utils {

std::vector<const char *> split(char * source, const char delimiter);

std::vector<const char *> file_contents(const char * path);

}
