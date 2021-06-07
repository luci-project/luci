#pragma once

#include <dlh/types.hpp>

template<size_t CAPACITY>
class ByteBuffer {
	uint8_t _buffer[CAPACITY];
	size_t _size;

 public:
	ByteBuffer() : _size(0) {}

	inline void * buffer() const {
		return _buffer;
	}

	inline size_t size() const {
		return _size;
	}

	inline size_t capacity() const {
		return CAPACITY;
	}

	template<typename T>
	inline size_t push(const T & v) {
		assert(_size + sizeof(T) < CAPACITY);
		*reinterpret_cast<T *>(_buffer + _size) = v;
		_size += sizeof(T);
		return sizeof(T);
	}

	template<typename T>
	inline T pop() {
		assert(_size >= sizeof(T));
		T * = reinterpret_cast<T *>(_buffer + _size);
		_size -= sizeof(T);
		return *T;
	}

	inline void clear() {
		_size = 0;
	}
};
