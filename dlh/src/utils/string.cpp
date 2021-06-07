#include <dlh/utils/string.hpp>

namespace String {

Vector<const char *> split(char * source, const char delimiter) {
	Vector<const char *> r;
	if (source != nullptr) {
		r.push_back(source);
		for (; *source != '\0'; ++source)
			if (*source == delimiter) {
				*source = '\0';
				r.push_back(source + 1);
			}
	}
	return r;
}

}  // namespace String
