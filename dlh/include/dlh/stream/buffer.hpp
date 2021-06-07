#pragma once

#include <dlh/types.hpp>

/*! \brief Sets the base for numbers
 * Shortcut functions are `bin`, `oct`, `dec` and `hex`
 */
struct setbase {
	unsigned base;
	setbase(unsigned base) : base(base) {}
};

/*! \brief Sets the field width to be used on output operations.
 * \note use zero to disable field width (no padding)
 */
struct setw {
	size_t n;
	setw(size_t n = 0) : n(n) {}
};

/*! \brief Sets the fill character
 */
struct setfill {
	const char c;
	setfill(char c) : c(c) {}
};


/*! \brief BufferStream class */
class BufferStream {
	/*! \brief Number system used for printing integral numbers (one of 2,
	 *  8, 10, or 16)
	 */
	unsigned base;

	/*! \brief Field width for next input */
	size_t width;

	/*! \brief Fill character */
	char fill;

	/*! \brief Upper or lowercase hex character */
	char hexchar;

	/*! Print base prefix */
	bool prefix;

	/*! Show sign for numbers (base 10) */
	enum { MINUS_ONLY, PLUS, SPACE } sign;

	/*! \brief Helper to write a string with fill characters (if necessary) */
	BufferStream& writefill(const char* string, size_t n);

	/*! \brief Helper to write an unsigned number with fill characters (if necessary) */
	BufferStream& writefill(unsigned long long ival, bool minus = false);


 protected:
	/*! \brief Buffer */
	char * bufptr;

	/*! \brief Length of buffer */
	size_t len;

	/*! \brief Current position of buffer */
	size_t pos;

 public:
	/*! \brief Default constructor. Initial number system is decimal. */
	BufferStream(char * buffer, size_t len) : bufptr(buffer), len(len), pos(0) {
		reset();
		buffer[len - 1] = '\0';
	}

	/*! \brief Destructor */
	virtual ~BufferStream() { flush(); }

	/*! \brief get current string */
	const char * str();

	/*! \brief Flush the buffer.
	 * (only for derived classes, has no effect in BufferStream)
	 */
	virtual void flush() {};

	/*! \brief Reset modifiers */
	void reset() {
		base = 10;
		width = 0;
		fill = ' ';
		hexchar = 'a';
		prefix = true;
		sign = MINUS_ONLY;
	}

	/*! \brief Write string (including all characters, even `\0`) into buffer
	 * \param string pointer to string with at least `n` bytes
	 * \param n number of bytes to copy from string into buffer
	 * \return Reference to BufferStream os; allows operator chaining.
	 */
	BufferStream& write(const char* string, size_t size);

	/*! \brief Write padding character multiple times into buffer
	 * \param c padding character
	 * \param n number of times the character should be written into buffer
	 * \return Reference to BufferStream os; allows operator chaining.
	 */
	BufferStream& write(char c, size_t num);

	/*! \brief Print a single character
	 *
	 *  \param c Character to be printed
	 *  \return Reference to BufferStream os; allows operator chaining.
	 */
	BufferStream& operator<<(char c);

	/*! \brief Print a single character
	 *
	 *  \param c Character to be printed
	 *  \return Reference to BufferStream os; allows operator chaining.
	 */
	BufferStream& operator<<(unsigned char c) {
		return *this << static_cast<char>(c);
	}

	/*! \brief Printing a null-terminated string
	 *
	 *  \param string String to be printed
	 *  \return Reference to BufferStream os; allows operator chaining.
	 */
	BufferStream& operator<<(const char* string);

	/*! \brief Print a boolean value
	 *
	 *  \param b Boolean to be printed
	 *  \return Reference to BufferStream os; allows operator chaining.
	 */
	BufferStream& operator<<(bool b) {
		return b ? writefill("true", 4) : writefill("false", 5);
	}

	/*! \brief Print an integral number in radix base
	 *
	 *  \param ival Number to be printed
	 *  \return Reference to BufferStream os; allows operator chaining.
	 */
	BufferStream& operator<<(short ival) {
		return *this << static_cast<long long>(ival);
	}

	/// \copydoc BufferStream::operator<<(short)
	BufferStream& operator<<(unsigned short ival) {
		return *this << static_cast<unsigned long long>(ival);
	}

	/// \copydoc BufferStream::operator<<(short)
	BufferStream& operator<<(int ival) {
		return *this << static_cast<long long>(ival);
	}

	/// \copydoc BufferStream::operator<<(short)
	BufferStream& operator<<(unsigned int ival) {
		return *this << static_cast<unsigned long long>(ival);
	}

	/// \copydoc BufferStream::operator<<(short)
	BufferStream& operator<<(long ival) {
		return *this << static_cast<long long>(ival);
	}

	/// \copydoc BufferStream::operator<<(short)
	BufferStream& operator<<(unsigned long ival) {
		return *this << static_cast<unsigned long long>(ival);
	}

	/// \copydoc BufferStream::operator<<(short)
	BufferStream& operator<<(long long ival) {
		if ((ival < 0) && (base == 10))
			return writefill(static_cast<unsigned long long>(-ival), true);
		else
			return writefill(static_cast<unsigned long long>(ival), false);
	}

	/// \copydoc BufferStream::operator<<(short)
	BufferStream& operator<<(unsigned long long ival) {
		return writefill(static_cast<unsigned long long>(ival), false);
	}

	/*! \brief Print a pointer as hexadecimal number
	 *
	 *  \param ptr Pointer to be printed
	 *  \return Reference to BufferStream os; allows operator chaining.
	 */
	BufferStream& operator<<(const void* ptr);

	/*! \brief Calls one of the manipulator functions.
	 *
	 *  Method that calls the manipulator functions defined below, which
	 *  allow modifying the stream's behavior by, for instance, changing the
	 *  number system.
	 *  \param f Manipulator function to be called
	 *  \return Reference to BufferStream os; allows operator chaining.
	 */
	BufferStream& operator<<(BufferStream& (*f) (BufferStream&)) {
		return f(*this);
	}

	/*! \brief Base manipulator
	 *  \param val Manipulator object to be used
	 *  \return Reference to BufferStream os; allows operator chaining.
	 */
	BufferStream& operator<<(const setbase & val);

	/*! \brief Field width manipulator
	 *  \param val Manipulator object to be used
	 *  \return Reference to BufferStream os; allows operator chaining.
	 */
	BufferStream& operator<<(const setw & val) {
		this->width = val.n;
		return *this;
	}

	/*! \brief Padding character manipulator
	 *  \param val Manipulator object to be used
	 *  \return Reference to BufferStream os; allows operator chaining.
	 */
	BufferStream& operator<<(const setfill & val) {
		this->fill = val.c;
		return *this;
	}

	/*! \brief Write format string
	 * \note Supports only a subset of printf format string
	 * \param format Format string
	 * \param args Arguments
	 * \return Bytes written
	 */
	size_t format(const char * format, ...);
};

/*! \brief Enforces a buffer flush.
 *  \param os Reference to stream to be flushed.
 *  \return Reference to BufferStream os; allows operator chaining.
 */
inline BufferStream& flush(BufferStream& os) {
	os.flush();
	return os;
}

/*! \brief Prints a newline character to the stream and issues a buffer flush.
 *  \param os Reference to stream to be modified.
 *  \return Reference to BufferStream os; allows operator chaining.
 */
inline BufferStream& endl(BufferStream& os) {
		return os << '\n' << flush;
}

/*! \brief Print subsequent numbers in binary form.
 *  \param os Reference to stream to be modified.
 *  \return Reference to BufferStream os; allows operator chaining.
 */
inline BufferStream& bin(BufferStream& os) {
	return os << setbase(2);
}

/*! \brief Print subsequent numbers in octal form.
 *  \param os Reference to stream to be modified.
 *  \return Reference to BufferStream os; allows operator chaining.
 */
inline BufferStream& oct(BufferStream& os) {
	return os << setbase(8);
}

/*! \brief Print subsequent numbers in decimal form.
 *  \param os Reference to stream to be modified.
 *  \return Reference to BufferStream os; allows operator chaining.
 */
inline BufferStream& dec(BufferStream& os) {
	return os << setbase(10);
}

/*! \brief Print subsequent numbers in hex form.
 *  \param os Reference to stream to be modified.
 *  \return Reference to BufferStream os; allows operator chaining.
 */
inline BufferStream& hex(BufferStream& os) {
	return os << setbase(16);
}
