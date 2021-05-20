#pragma once

#include <cstddef>
#include <vector>

class BufferStream {
 protected:
	/*! \brief Buffer */
	char * bufptr;

	/*! \brief Length of buffer */
	size_t len;

	/*! \brief Current position of buffer */
	size_t pos;

 private:
	friend BufferStream& bin(BufferStream& os);
	friend BufferStream& oct(BufferStream& os);
	friend BufferStream& dec(BufferStream& os);
	friend BufferStream& hex(BufferStream& os);

	/*! \brief Number system used for printing integral numbers (one of 2,
	 *  8, 10, or 16)
	 */
	int base;


 public:
	/*! \brief Default constructor. Initial number system is decimal. */
	BufferStream(char * buffer, size_t len) : bufptr(buffer), len(len), pos(0), base(10) {
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
	BufferStream& operator<<(unsigned char c);

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
	BufferStream& operator<<(bool b);

	/*! \brief Print an integral number in radix base
	 *
	 *  \param ival Number to be printed
	 *  \return Reference to BufferStream os; allows operator chaining.
	 */
	BufferStream& operator<<(short ival);

	/// \copydoc BufferStream::operator<<(short)
	BufferStream& operator<<(unsigned short ival);

	/// \copydoc BufferStream::operator<<(short)
	BufferStream& operator<<(int ival);

	/// \copydoc BufferStream::operator<<(short)
	BufferStream& operator<<(unsigned int ival);

	/// \copydoc BufferStream::operator<<(short)
	BufferStream& operator<<(long ival);

	/// \copydoc BufferStream::operator<<(short)
	BufferStream& operator<<(unsigned long ival);

	/// \copydoc BufferStream::operator<<(short)
	BufferStream& operator<<(long long ival);

	/// \copydoc BufferStream::operator<<(short)
	BufferStream& operator<<(unsigned long long ival);

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
	BufferStream& operator<<(BufferStream& (*f) (BufferStream&));


	template<typename T>
	BufferStream& operator<<(std::vector<T> & val) {
		*this << "{ ";
		bool p = false;
		for (const auto & v : val) {
			if (p)
				*this << ", ";
			else
				p = true;
			*this << v;
		}
		return *this << '}';
	}
};

/*! \brief Enforces a buffer flush.
 *  \param os Reference to stream to be flushed.
 *  \return Reference to BufferStream os; allows operator chaining.
 */
BufferStream& flush(BufferStream& os);


/*! \brief Prints a newline character to the stream and issues a buffer flush.
 *  \param os Reference to stream to be modified.
 *  \return Reference to BufferStream os; allows operator chaining.
 */
BufferStream& endl(BufferStream& os);

/*! \brief Print subsequent numbers in binary form.
 *  \param os Reference to stream to be modified.
 *  \return Reference to BufferStream os; allows operator chaining.
 */
BufferStream& bin(BufferStream& os);

/*! \brief Print subsequent numbers in octal form.
 *  \param os Reference to stream to be modified.
 *  \return Reference to BufferStream os; allows operator chaining.
 */
BufferStream& oct(BufferStream& os);

/*! \brief Print subsequent numbers in decimal form.
 *  \param os Reference to stream to be modified.
 *  \return Reference to BufferStream os; allows operator chaining.
 */
BufferStream& dec(BufferStream& os);

/*! \brief Print subsequent numbers in hex form.
 *  \param os Reference to stream to be modified.
 *  \return Reference to BufferStream os; allows operator chaining.
 */
BufferStream& hex(BufferStream& os);
