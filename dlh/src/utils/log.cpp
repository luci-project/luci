#include <dlh/utils/log.hpp>
#include <dlh/errno.hpp>
#include <dlh/unistd.hpp>

#include <fcntl.h>


bool Log::output(int fd) {
	if (fcntl(fd, F_GETFD) != -1 || errno != EBADF) {
		this->fd = fd;
		return true;
	} else {
		return false;
	}
}

bool Log::output(const char * file, bool truncate) {
	if (file != nullptr) {
		int fd = open(file, O_CREAT | O_WRONLY | O_CLOEXEC | (truncate ? O_TRUNC : O_APPEND));
		if (fd > 0) {
			if (this->fd > 2)
				close(this->fd);
			this->fd = fd;
			return true;
		}
	}
	return false;
}

void Log::flush() {
	if (severity <= limit && severity != Level::NONE && pos > 0) {
		if (fd <= 2)
			*this << "\e[0m";
		OutputStream::flush();
	} else {
		// Drop message
		pos = 0;
	}
}

static const char * level_name[] = {
	nullptr,
	"\e[41;30;1m FATAL \e[0;40;31m",
	"\e[41;30m ERROR \e[40;31m",
	"\e[43;30mWARNING\e[40;33m",
	"\e[47;30;1m INFO  \e[0;40;37m",
	"\e[47;30mVERBOSE\e[40;37m",
	"\e[7;1m DEBUG \e[0;40m",
	"\e[7m TRACE \e[0;40m"
};

Log& Log::entry(Level level, const char * file, unsigned line) {
	flush();
	severity = level;
	if (level <= limit && level > NONE) {
		// STDOUT / STDERR with ANSI color codes
		if (fd <= 2) {
			*this << level_name[level];
			size_t p = pos;
			if (file != nullptr) {
				*this << ' ' << file;
				if (line > 0)
					*this << ':' << line;
			}
			*this << ' ';
			// Pad file name
			while (pos - p < 32)
				*this << '.';
			*this << "\e[49m ";
		} else {
			// For files
			*this << level << '\t' << file << '\t' << line << '\t';
		}
	}
	return *this;
}

Log logger;
