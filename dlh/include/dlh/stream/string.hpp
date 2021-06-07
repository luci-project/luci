#pragma once

#include <dlh/types.hpp>
#include <dlh/stream/buffer.hpp>

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
