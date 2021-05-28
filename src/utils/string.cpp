#include "utils/string.hpp"

namespace String {

std::vector<const char *> split(char * source, const char delimiter) {
	std::vector<const char *> r;
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
