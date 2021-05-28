#pragma once

#include <vector>

namespace Parser {

bool string(unsigned long long & target, const char * value);

bool string(long long & target, const char * value);

bool string(unsigned long & target, const char * value);

bool string(long & target, const char * value);

bool string(unsigned & target, const char * value);

bool string(int & target, const char * value);

bool string(unsigned short & target, const char * value);

bool string(short & target, const char * value);

bool string(bool & target, const char * value);

bool string(const char * & target, const char * value);

template<typename T>
bool string(std::vector<T> & target, const char * value) {
	T tmp;
	bool r = string(tmp, value);
	target.push_back(tmp);
	return r;
}

}  // namespace Parse
