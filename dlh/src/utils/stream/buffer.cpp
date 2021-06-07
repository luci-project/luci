#include <dlh/stream/buffer.hpp>
#include <dlh/types.hpp>
#include <dlh/string.hpp>

#include <cstdarg>

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
	if ((base == 10 && (minus || sign != MINUS_ONLY)) || (base == 8 && prefix))
		len += 1;
	else if ((base == 2 || base == 16) && prefix)
		len += 2;

	if (width > len && (fill != '0' || base == 10))
		write(fill, width - len);
	width = 0;

	// Print prefix
	switch(base) {
		case 2:
			if (prefix) operator<<("0b");
			break;

		case 8:
			if (prefix) operator<<('0');
			break;

		case 16:
			if (prefix) operator<<("0x");
			break;

		default:
			if (minus)
				operator<<('-');
			else if (sign == PLUS)
				operator<<('+');
			else if (sign == SPACE)
				operator<<(' ');
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
			operator<<(static_cast<char>(hexchar + digit - 10));

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

size_t BufferStream::format(const char * format, ...) {
	va_list arguments;
	va_start(arguments, format);

	size_t start = pos;
	enum {
		NONE,
		MARKER,
		FLAGS,
		WIDTHSTAR,
		WIDTH,
		LENGTH,
		SPECIFIER
	} state = NONE;
	enum {
		SHORT,
		INT,
		LONG,
		LONGLONG,
	} length = INT;
	while (*format) {
		switch (state) {
			case MARKER:
				if (*format == '%') {
					state = NONE;
					operator<<(*format);
					break;
				} else {
					state = FLAGS;
				}
				[[fallthrough]];

			case FLAGS:
				if (*format == '+') {
					sign = PLUS;
					break;
				} else if (*format == ' ') {
					sign = SPACE;
					break;
				} else if (*format == '0') {
					fill = '0';
					break;
				} else if (*format == '#') {
					prefix = true;
					break;
				} else {
					state = WIDTHSTAR;
				}
				[[fallthrough]];

			case WIDTHSTAR:
				if (*format == '*') {
					width = va_arg(arguments, size_t);
					state = LENGTH;
					break;
				} else {
					state = WIDTH;
				}
				[[fallthrough]];

			case WIDTH:
				if (*format >= '0' && *format <= '9') {
					width = width * 10 + *format - '0';
					break;
				} else {
					state = LENGTH;
				}
				[[fallthrough]];

			// Precision is not supported

			case LENGTH:
				switch (*format) {
					case 'h':
						// hh not supported
						length = SHORT;
						break;
					case 'l':
						length = length != LONG ? LONG : LONGLONG;
						break;
					case 'j':
						length = sizeof(intmax_t) == sizeof(long) ? LONG : LONGLONG;
						break;
					case 'z':
						length = sizeof(size_t) == sizeof(long) ? LONG : LONGLONG;
						break;
					case 't':
						length = sizeof(ptrdiff_t) == sizeof(long) ? LONG : LONGLONG;
						break;
					default:
						state = SPECIFIER;
				}
				if (state != SPECIFIER)
					break;
				[[fallthrough]];

			case SPECIFIER:
				switch (*format) {
					case 'c':
						operator<<(static_cast<char>(va_arg(arguments, int)));
						state = NONE;
						break;

					case 's':
						operator<<(va_arg(arguments, const char *));
						state = NONE;
						break;

					case 'p':
						operator<<(va_arg(arguments, void *));
						state = NONE;
						break;

					case 'n':
						*va_arg(arguments, int *) = pos - start;
						state = NONE;
						break;

					case 'd':
					case 'i':
						switch (length) {
							case SHORT:
								operator<<(static_cast<short>(va_arg(arguments, int)));
								break;

							case LONG:
								operator<<(va_arg(arguments, long));
								break;

							case LONGLONG:
								operator<<(va_arg(arguments, long long));
								break;

							default:
								operator<<(va_arg(arguments, int));
						}
						state = NONE;
						break;

					case 'o':
						base = 8;
						break;

					case 'x':
						hexchar = 'a';
						base = 16;
						break;

					case 'X':
						hexchar = 'A';
						base = 16;
						break;

					case 'u':
						break;

					default:
						state = NONE;
				}
				if (state != NONE)
					switch (length) {
						case SHORT:
							operator<<(static_cast<unsigned short>(va_arg(arguments, unsigned)));
							break;

						case LONG:
							operator<<(va_arg(arguments, unsigned long));
							break;

						case LONGLONG:
							operator<<(va_arg(arguments, unsigned long long));
							break;

						default:
							operator<<(va_arg(arguments, unsigned));
					}
				state = NONE;
				break;

			default:
				if (*format == '%') {
					state = MARKER;
					width = 0;
					fill = ' ';
					prefix = false;
					sign = MINUS_ONLY;
					length = INT;
				} else {
					operator<<(*format);
				}
		}
		format++;
	}
	va_end(arguments);

	// Reset modifiers
	reset();
	return pos - start;
}
