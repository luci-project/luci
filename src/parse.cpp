#include "parse.hpp"

#include <climits>

namespace Parse {

bool string(unsigned long long & target, const char * value) {
	for(target = 0; value != nullptr && *value != '\0'; value++)
		if (*value >= '0' && *value <= '9') {
			unsigned long long o = target;
			target = target * 10 + *value - '0';
			if (target < o)
				return false;
		} else if (*value != ' ' && *value != ',' && *value != '\'') {
			return false;
		}

	return true;
}

bool string(long long & target, const char * value) {
	if (value != nullptr) {
		for (target = 1; *value != '\0'; value++)
			if (*value == '-')  {
				target = -1;
			} else if (*value >= '0' && *value <= '9') {
				break;
			} else if (*value != ' ') {
				return false;
			}

		unsigned long long v = 0;
		bool r = string(v, value);
		target *= v;
		return r;
	} else {
		target = 0;
		return true;
	}
}

bool string(unsigned long & target, const char * value) {
	unsigned long long v = 0;
	bool r = string(v, value);
	target = static_cast<unsigned long>(v);
	return v > ULONG_MAX ? false : r ;
}

bool string(long & target, const char * value) {
	long long v = 0;
	bool r = string(v, value);
	target = static_cast<long>(v);
	return v > LONG_MAX || v < LONG_MIN ? false : r ;
}

bool string(unsigned & target, const char * value) {
	unsigned long long v = 0;
	bool r = string(v, value);
	target = static_cast<unsigned>(v);
	return v > UINT_MAX ? false : r ;
}

bool string(int & target, const char * value) {
	long long v = 0;
	bool r = string(v, value);
	target = static_cast<int>(v);
	return v > INT_MAX || v < INT_MIN ? false : r ;
}

bool string(unsigned short & target, const char * value) {
	unsigned long long v = 0;
	bool r = string(v, value);
	target = static_cast<unsigned short>(v);
	return v > USHRT_MAX ? false : r ;
}

bool string(short & target, const char * value) {
	long long v = 0;
	bool r = string(v, value);
	target = static_cast<short>(v);
	return v > SHRT_MAX || v < SHRT_MIN ? false : r ;
}

bool string(bool & target, const char * value) {
	target = (*value != '\0' && *value != '0');
	return true;
}

bool string(const char * & target, const char * value) {
	target = value;
	return true;
}

}  // namespace Parse
