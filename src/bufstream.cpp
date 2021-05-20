#include "bufstream.hpp"

const char * BufferStream::str() {
	if (len < 1)
		return nullptr;
	else if (pos >= len)
		pos = len - 1;
	bufptr[pos] = '\0';
	return bufptr;
}

// Print a single character (trivial)
BufferStream& BufferStream::operator<<(char c) {
	if (pos + 1 < len)
		bufptr[pos++] = c;
	if (pos + 1 >= len)
		flush();
	return *this;
}

BufferStream& BufferStream::operator<<(unsigned char c) {
	return *this << static_cast<char>(c);
}

// Printing a null-terminated string
BufferStream& BufferStream::operator<<(const char* string) {
	if (string != nullptr)
		while ((*string) != '\0' && pos + 1 < len) {
			bufptr[pos++] = *(string++);
			if (pos + 1 == len)
				flush();
		}
	return *this;
}

BufferStream& BufferStream::operator<<(bool b) {
	return *this << (b ? "true" : "false");
}

// Print integral numbers in number system base.
// All signed types are promoted to long long,
// all unsigned types to unsigned long long.

BufferStream& BufferStream::operator<<(short ival) {
	return *this << static_cast<long long>(ival);
}

BufferStream& BufferStream::operator<<(unsigned short ival) {
	return *this << static_cast<unsigned long long>(ival);
}

BufferStream& BufferStream::operator<<(int ival) {
	return *this << static_cast<long long>(ival);
}

BufferStream& BufferStream::operator<<(unsigned int ival) {
	return *this << static_cast<unsigned long long>(ival);
}

BufferStream& BufferStream::operator<<(long ival) {
	return *this << static_cast<long long>(ival);
}

BufferStream& BufferStream::operator<<(unsigned long ival) {
	return *this << static_cast<unsigned long long>(ival);
}

// Print a signed , integral number.
BufferStream& BufferStream::operator<<(long long ival) {
	if ((ival < 0) && (base == 10)) {
		operator<<('-');
		ival = -ival;
	}
	// Print the remaining positive number using the unsigned output
	return *this << static_cast<unsigned long long>(ival);
}

// Print a unsigned, integral number.
BufferStream& BufferStream::operator<<(unsigned long long ival) {
	switch(base) {
		case 2:
			operator<<("0b");
			break;

		case 8:
			operator<<('0');
			break;

		case 10:
			break;

		case 16:
			operator<<("0x");
			break;

		default:
			base = 10;
	}

	// Determine the largest potency in the number system used, which is
	// still smaller than the number to be printed
	unsigned long long div;
	for (div = 1; ival/div >= static_cast<unsigned long long>(base); div *= base) {}

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

// Print a pointer as hexadecimal number
BufferStream& BufferStream::operator<<(const void* ptr) {
	int oldbase = base;
	base = 16;
	*this << reinterpret_cast<unsigned long long>(ptr);
	base = oldbase;
	return *this;
}

// Calls one of the manipulator functions
BufferStream& BufferStream::operator<<(BufferStream& (*f) (BufferStream&)) {
	return f(*this);
}

// flush: Explicit buffer flush
BufferStream& flush(BufferStream& os) {
	os.flush();
	return os;
}

// endl: Inserts a newline to the output
BufferStream& endl(BufferStream& os) {
	os << '\n' << flush;
	return os;
}

// bin: Selects the binary number system
BufferStream& bin(BufferStream& os) {
	os.base = 2;
	return os;
}

// oct: Selects the octal number system
BufferStream& oct(BufferStream& os) {
	os.base = 8;
	return os;
}

// dec: Selects the decimal number system
BufferStream& dec(BufferStream& os) {
	os.base = 10;
	return os;
}

// hex: Selects the hexadecimal number system
BufferStream& hex(BufferStream& os) {
	os.base = 16;
	return os;
}
