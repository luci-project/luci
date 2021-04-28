#include "utils.hpp"

#include <sys/mman.h>

#include "generic.hpp"

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
		LOG_ERROR << "Reading file " << path << " failed: " << strerror(errno);
		return {};
	}

	struct stat sb;
	if (::fstat(fd, &sb) == -1) {
		LOG_ERROR << "Stat file " << path << " failed: " << strerror(errno);
		::close(fd);
		return {};
	}
	size_t length = sb.st_size;

	void * addr = ::mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	::close(fd);
	if (addr == MAP_FAILED) {
		LOG_ERROR << "Mmap file " << path << " failed: " << strerror(errno);
		return {};
	} else {
		LOG_VERBOSE << "Mapped '" << path << "' (" << length << " bytes)";
		return split(reinterpret_cast<char *>(addr), '\n');
	}
}

}  // namespace Utils
