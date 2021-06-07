#include <dlh/utils/environ.hpp>

extern char **environ;

namespace Environ {

char * variable(const char * name, bool consume) {
	for (char ** ep = environ; *ep != nullptr; ep++) {
		char * e = *ep;
		const char * n = name;
		while (*n == *e && *e != '\0') {
			n++;
			e++;
		}
		if (*e == '=' && *n == '\0') {
			if (consume)
				**ep = '\0';
			return e + 1;
		}
	}
	return nullptr;
}

}  // namespace Environ
