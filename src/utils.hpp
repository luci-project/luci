#pragma once

#include <string>
#include <vector>

namespace Utils {

std::vector<std::string> split(const std::string & source, const char delimiter);

std::vector<std::string> file_contents(const std::string & path);

void * mmap_file(const char * path, int & fd, size_t & length);


}
