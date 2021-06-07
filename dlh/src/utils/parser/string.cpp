#include <dlh/parser/string.hpp>
#include <dlh/types.hpp>

namespace Parser {

bool string(uint64_t & target, const char * value) {
	for(target = 0; value != nullptr && *value != '\0'; value++)
		if (*value >= '0' && *value <= '9') {
			uint64_t o = target;
			target = target * 10 + *value - '0';
			if (target < o)
				return false;
		} else if (*value != ' ' && *value != ',' && *value != '\'') {
			return false;
		}

	return true;
}

bool string(int64_t & target, const char * value) {
	if (value != nullptr) {
		for (target = 1; *value != '\0'; value++)
			if (*value == '-')  {
				target = -1;
			} else if (*value >= '0' && *value <= '9') {
				break;
			} else if (*value != ' ') {
				return false;
			}

		uint64_t v = 0;
		bool r = string(v, value);
		target *= v;
		return r;
	} else {
		target = 0;
		return true;
	}
}

bool string(uint32_t & target, const char * value) {
	uint64_t v = 0;
	bool r = string(v, value);
	target = static_cast<uint32_t>(v);
	return v > UINT32_MAX ? false : r ;
}

bool string(int32_t & target, const char * value) {
	int64_t v = 0;
	bool r = string(v, value);
	target = static_cast<int32_t>(v);
	return v > INT32_MAX || v < INT32_MIN ? false : r ;
}

bool string(uint16_t & target, const char * value) {
	uint64_t v = 0;
	bool r = string(v, value);
	target = static_cast<uint16_t>(v);
	return v > UINT16_MAX ? false : r ;
}

bool string(int16_t & target, const char * value) {
	int64_t v = 0;
	bool r = string(v, value);
	target = static_cast<int16_t>(v);
	return v > INT16_MAX || v < INT16_MIN ? false : r ;
}

bool string(uint8_t & target, const char * value) {
	uint64_t v = 0;
	bool r = string(v, value);
	target = static_cast<uint8_t>(v);
	return v > UINT8_MAX ? false : r ;
}

bool string(int8_t & target, const char * value) {
	int64_t v = 0;
	bool r = string(v, value);
	target = static_cast<int8_t>(v);
	return v > INT8_MAX || v < INT8_MIN ? false : r ;
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
