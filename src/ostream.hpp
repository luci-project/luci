#pragma once

#include <cstddef>
#include <unistd.h>

#include "bufstream.hpp"

template<size_t BUFFERSZ>
class OutputStream : public BufferStream {
 protected:
	char buffer[BUFFERSZ];

	int fd;

 public:
	/*! \brief Default constructor  */
	explicit OutputStream(int fd) : BufferStream(buffer, BUFFERSZ), fd(fd) {}

	/*! \brief Destructor */
	virtual ~OutputStream() {}

	/*! \brief Flush the buffer.
	 */
	virtual void flush() override {
		size_t len = 0;
		while (len < pos) {
			ssize_t r = ::write(fd, buffer + len, pos - len);
			if (r < 0)
				break;
			else
				len += r;
		}
		pos = 0;
	}

};

extern OutputStream<1024> cout, cerr;
