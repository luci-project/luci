#pragma once

#include <cstddef>
#include <unistd.h>

#include "bufstream.hpp"

template<size_t BUFFERSZ>
class StringStream : public BufferStream {
 protected:
	char buffer[BUFFERSZ];

 public:
	/*! \brief Default constructor  */
	explicit StringStream() : BufferStream(buffer, BUFFERSZ) {}

	/*! \brief Destructor */
	virtual ~StringStream() {}
};
