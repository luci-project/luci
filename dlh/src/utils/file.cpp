#include <dlh/utils/file.hpp>
#include <dlh/errno.hpp>
#include <dlh/unistd.hpp>
#include <dlh/utils/log.hpp>
#include <dlh/utils/string.hpp>

namespace File {

bool exists(const char * path) {
	return ::access(path, F_OK ) == 0;
}

bool readable(const char * path) {
	return ::access(path, R_OK ) == 0;
}

bool writeable(const char * path) {
	return ::access(path, W_OK ) == 0;
}

bool executable(const char * path) {
	return ::access(path, X_OK ) == 0;
}

Vector<const char *> contents(const char * path) {
	errno = 0;
	int fd = ::open(path, O_RDONLY);
	if (fd < 0) {
		LOG_ERROR << "Reading file " << path << " failed: " << strerror(errno) << endl;
		return Vector<const char *>();
	}

	struct stat sb;
	if (::fstat(fd, &sb) == -1) {
		LOG_ERROR << "Stat file " << path << " failed: " << strerror(errno) << endl;
		::close(fd);
		return Vector<const char *>();
	}
	size_t length = sb.st_size;

	void * addr = ::mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	::close(fd);
	if (addr == MAP_FAILED) {
		LOG_ERROR << "Mmap file " << path << " failed: " << strerror(errno) << endl;
		return Vector<const char *>();
	} else {
		LOG_VERBOSE << "Mapped '" << path << "' (" << length << " bytes)" << endl;
		return String::split(reinterpret_cast<char *>(addr), '\n');
	}
}

}  // Namespace File
