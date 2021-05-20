#include "utils.hpp"

#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "generic.hpp"


extern char **environ;

namespace Utils {

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

std::vector<const char *> file_contents(const char * path) {
	errno = 0;
	int fd = ::open(path, O_RDONLY);
	if (fd < 0) {
		LOG_ERROR << "Reading file " << path << " failed: " << strerror(errno) << endl;
		return {};
	}

	struct stat sb;
	if (::fstat(fd, &sb) == -1) {
		LOG_ERROR << "Stat file " << path << " failed: " << strerror(errno) << endl;
		::close(fd);
		return {};
	}
	size_t length = sb.st_size;

	void * addr = ::mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	::close(fd);
	if (addr == MAP_FAILED) {
		LOG_ERROR << "Mmap file " << path << " failed: " << strerror(errno) << endl;
		return {};
	} else {
		LOG_VERBOSE << "Mapped '" << path << "' (" << length << " bytes)" << endl;
		return split(reinterpret_cast<char *>(addr), '\n');
	}
}

char * env(const char * name, bool consume) {
	for (char ** ep = environ; *ep != NULL; ep++) {
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

Auxiliary aux(Auxiliary::type type) {
	static int envc = -1;
	if (envc == -1)
		for (envc = 0; environ[envc] != nullptr; envc++) {}

	// Read current auxiliary vectors
	Auxiliary * auxv = reinterpret_cast<Auxiliary *>(environ + envc + 1);
	for (int auxc = 0 ; auxv[auxc].a_type != Auxiliary::AT_NULL; auxc++)
		if (auxv[auxc].a_type == type)
			return auxv[auxc];

	return {};
}

template<>
bool parse(unsigned long long & element, const char * value) {
	for(element = 0; value != nullptr && *value != '\0'; value++)
		if (*value >= '0' && *value <= '9') {
			unsigned long long o = element;
			element = element * 10 + *value - '0';
			if (element < o)
				return false;
		} else if (*value != ' ' && *value != '\t' && *value != '.' && *value != '\'') {
			return false;
		}

	return true;
}

template<>
bool parse(long long & element, const char * value) {
	if (value != nullptr) {
		for (element = 1; *value != '\0'; value++)
			if (*value == '-')  {
				element = -1;
			} else if (*value >= '0' && *value <= '9') {
				break;
			} else if (*value != ' ' && *value != '\t') {
				return false;
			}

		unsigned long long v = 0;
		bool r = parse(v, value);
		element *= v;
		return r;
	} else {
		element = 0;
		return true;
	}
}

template<>
bool parse(unsigned & element, const char * value) {
	unsigned long long v = 0;
	bool r = parse(v, value);
	element = static_cast<unsigned>(v);
	return v > UINT_MAX ? false : r ;
}

template<>
bool parse(int & element, const char * value) {
	long long v = 0;
	bool r = parse(v, value);
	element = static_cast<int>(v);
	return v > INT_MAX || v < INT_MIN ? false : r ;
}

template<>
bool parse(const char * & element, const char * value) {
	element = value;
	return true;
}

template<>
bool parse(bool & element, const char * value) {
	element = (*value != '\0' && *value != '0');
	return true;
}

}  // namespace Utils
