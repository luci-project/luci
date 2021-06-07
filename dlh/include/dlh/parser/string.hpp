#pragma once

#include <dlh/container/vector.hpp>

namespace Parser {

bool string(uint64_t & target, const char * value);

bool string(int64_t & target, const char * value);

bool string(uint32_t & target, const char * value);

bool string(int32_t & target, const char * value);

bool string(uint16_t & target, const char * value);

bool string(int16_t & target, const char * value);

bool string(uint8_t & target, const char * value);

bool string(int8_t & target, const char * value);

bool string(bool & target, const char * value);

bool string(const char * & target, const char * value);

template<typename T>
bool string(Vector<T> & target, const char * value) {
	T tmp;
	bool r = string(tmp, value);
	target.push_back(tmp);
	return r;
}

}  // namespace Parse
