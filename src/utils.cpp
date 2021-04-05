#include "utils.hpp"

#include <sys/mman.h>
#include <ext/stdio_filebuf.h>

#include "generic.hpp"

namespace Utils {

std::vector<std::string> split(const std::string & source, const char delimiter) {
	std::vector<std::string> r;
	size_t i, l = 0;
	while ((i = source.find(delimiter, l)) != source.npos) {
		auto s = source.substr(l, i - l);
		if (!s.empty())
			r.push_back(s);
		l = i + 1;
	}
	r.push_back(source.substr(l));

	return r;
}

std::vector<std::string> file_contents(const std::string & path) {

	int fd = -1;
	size_t length = 0;
	void * addr = mmap_file(path.c_str(), fd, length);

	if (addr != nullptr) {
		LOG_DEBUG << "Mapped '" << path << "' (" << length << " bytes)";
		std::string lines(reinterpret_cast<const char *>(addr));
		return split(lines, '\n');
	} else {
		return std::vector<std::string>();
	}
}

void * mmap_file(const char * path, int & fd, size_t & length) {
	errno = 0;
	if (fd < 0 && (fd = ::open(path, O_RDONLY)) < 0) {
		LOG_ERROR << "Reading file " << path << " failed: " << strerror(errno);
		return nullptr;
	}

	// Determine file size
	if (length == 0) {
		struct stat sb;
		if (::fstat(fd, &sb) == -1) {
			LOG_ERROR << "Stat file " << path << " failed: " << strerror(errno);
			::close(fd);
			return nullptr;
		}
		length = sb.st_size;
	}

	// Map file
	void * addr = ::mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
	if (addr == MAP_FAILED) {
		LOG_ERROR << "Mmap file " << path << " failed: " << strerror(errno);
		::close(fd);
		return nullptr;
	} else {
		return addr;
	}
}

}
