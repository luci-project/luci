#include "bufstream.hpp"

#include <cstring>

const char * BufferStream::str() {
	if (len < 1)
		return nullptr;
	else if (pos >= len)
		pos = len - 1;
	bufptr[pos] = '\0';
	return bufptr;
}

BufferStream& BufferStream::writefill(const char* string, size_t n) {
	if (width > n)
		write(fill, width - n);
	width = 0;

	return write(string, n);
}

// Print a bool number.
BufferStream& BufferStream::writefill(unsigned long long ival, bool minus) {
	// Determine the largest potency in the number system used, which is
	// still smaller than the number to be printed
	size_t len = 1;
	unsigned long long div;
	for (div = 1; ival/div >= static_cast<unsigned long long>(base); div *= base) { len++; }

	// Fill (if necessary)
	if (base == 8 || (base == 10 && minus))
		len += 1;
	else if (base == 2 || base == 16)
		len += 2;

	if (width > len && (fill != '0' || base == 10))
		write(fill, width - len);
	width = 0;

	// Print prefix
	switch(base) {
		case 2:
			operator<<("0b");
			break;

		case 8:
			operator<<('0');
			break;

		case 16:
			operator<<("0x");
			break;

		default:
			if (minus)
				operator<<('-');
	}
	// Special case: Zeros after prefix (for base != 10)
	if (width > len && fill == '0' && base != 10)
		write(fill, width - len);
	width = 0;

	// print number char by char
	for (; div > 0; div /= static_cast<unsigned long long>(base)) {
		auto digit = ival / div;
		if (digit < 10)
			operator<<(static_cast<char>('0' + digit));
		else
			operator<<(static_cast<char>('a' + digit - 10));

		ival %= div;
	}
	return *this;
}

BufferStream& BufferStream::write(const char* string, size_t n) {
	for (size_t i = 0; i < n; i++)
		if (pos + 1 < len) {
			bufptr[pos++] = string[i];
			if (pos + 1 == len)
				flush();
		}

	return *this;
}

BufferStream& BufferStream::write(char c, size_t n) {
	for (size_t i = 0; i < n; i++)
		if (pos + 1 < len) {
			bufptr[pos++] = c;
			if (pos + 1 == len)
				flush();
		}
	return *this;
}

// Print a single character (trivial)
BufferStream& BufferStream::operator<<(char c) {
	if (width > 1)
		write(fill, width - 1);
	width = 0;

	if (pos + 1 < len)
		bufptr[pos++] = c;
	if (pos + 1 >= len)
		flush();
	return *this;
}

// Printing a null-terminated string
BufferStream& BufferStream::operator<<(const char* string) {
	if (string != nullptr) {
		if (width > 0)
			return writefill(string, strlen(string));
		else  // slightly faster
			while ((*string) != '\0' && pos + 1 < len) {
				bufptr[pos++] = *(string++);
				if (pos + 1 == len)
					flush();
			}
	}
	return *this;
}


// Print a pointer as hexadecimal number
BufferStream& BufferStream::operator<<(const void* ptr) {
	int oldbase = base;
	base = 16;
	*this << reinterpret_cast<unsigned long long>(ptr);
	base = oldbase;
	return *this;
}


// Calls one of the manipulator functions
BufferStream& BufferStream::operator<<(const setbase & val) {
	switch(val.base) {
		case 2:
		case 8:
		case 10:
		case 16:
			this->base = val.base;
		default:
			this->base = 10;
	}
	return *this;
}
