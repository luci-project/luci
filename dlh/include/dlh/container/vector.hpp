/*! \file
 *  \brief vector class
 */
#pragma once

#include <dlh/types.hpp>
#include <dlh/utility.hpp>
#include <dlh/stream/buffer.hpp>

/*! \brief Vector class influenced by standard [template/cxx] library
 */
template<class T> class Vector {
	/*! \brief Array with entries
	 * dynamically allocated / increased
	 */
	T* _element;

	/*! \brief Number of entries
	 */
	size_t _size;

	/*! \brief Current maximum capacity
	 */
	size_t _capacity;

	/*! \brief Return object for out of bounds
	 * since we don't have exceptions
	 */
	T _invalid;

	/*! \brief Expand capacity
	 */
	inline void expand() {
		reserve(0 < _capacity ? 2 * _capacity : 16);
	}

 public:
	/*! \brief Constructor
	 * \param invalid object for invalid returns
	 */
	explicit Vector(T invalid = T()) : _element(nullptr), _size(0), _capacity(0), _invalid(invalid) {}

	/*! \brief Constructor with initial _capacity
	 * \param count number of initial values
	 * \param init initial values
	 * \param invalid object for invalid returns
	 */
	explicit Vector(size_t count, T init, T invalid = T()) : _element(new T[count]), _size(count),
	                                                         _capacity(count), _invalid(invalid) {
		for (size_t i = 0; i < count; ++i) {
			_element[i] = init;
		}
	}

	/*! \brief Copy constructor
	 */
	Vector(const Vector& other) : _element(new T[other._capacity]), _size(other._size),
	                              _capacity(other._capacity), _invalid(other._invalid) {
		for (size_t i = 0; i < _size; ++i) {
			_element[i] = other._element[i];
		}
	}

	/*! \brief Destructor
	 */
	~Vector() {
		delete[] _element;
	}

	/*! \brief access specified element
	 * \param pos position of the element to return
	 * \return element or T() if out of bounds
	 * \note This function differs to the standard library due to the lack of exceptions
	 */
	inline T& at(size_t pos) {
		return pos < _size ? _element[pos] : _invalid;
	}

	/*! \brief Access the first element
	 * \return first element or T() if none
	 */
	inline T& front() {
		return 0 == _size ? _element[0] : _invalid;
	}

	/*! \brief Access the last element
	 * \return last element or T() if none
	 */
	inline T& back() {
		return 0 == _size ? _element[_size - 1] : _invalid;
	}

	/*! \brief direct access to the underlying array
	 * \return Pointer to data
	 */
	inline T* data() {
		return _element;
	}

	/*! \brief Checks if the container has no element
	 */
	inline bool empty() const {
		return _size == 0;
	}

	/*! \brief Get the number of elements
	 * \return number of entires
	 */
	inline size_t size() const {
		return _size;
	}

	/*! \brief Get the number of elements that can be held in currently allocated storage
	 * \return current capacity
	 */
	inline size_t capacity() const {
		return _capacity;
	}

	/*! \brief Erases all elements from the container
	 */
	inline void clear() {
		_size = 0;
	}

	/*! \brief Increase the capacity of the vector
	 * \param capacity new capacity
	 */
	inline void reserve(size_t capacity) {
		if (capacity > this->_capacity) {
			T* tmp = new T[capacity];

			for (size_t i = 0; i < _size; ++i) {
				tmp[i] = _element[i];
			}

			delete[] _element;
			_element = tmp;
			this->_capacity = capacity;
		}
	}

	/*! \brief Resizes the container
	 * \param count new size of the container
	 * \param value the value to initialize the new elements with
	 */
	inline void resize(size_t count, T value) {
		reserve(count);
		for (size_t i = _size; i < count; ++i) {
			_element[i] = value;
		}
		_size = count;
	}

	/*! \brief Resizes the container
	 * \param count new size of the container
	 */
	inline void resize(size_t count) {
		resize(count, _invalid);
	}

	/*! \brief Creates an element at the end
	 * \param args Arguments to create the value
	 */
	template<typename... ARGS>
	inline void emplace_back(ARGS&&... args) {
		if (_capacity == _size) {
			expand();
		}

		_element[_size] = move(T(forward<ARGS>(args)...));
		_size++;
	}

	/*! \brief Adds an element to the end
	 * \param value the value of the element to append
	 */
	inline void push_back(const T& value) {
		emplace_back(value);
	}

	/*! \brief Adds an element to the end
	 * \param value the value of the element to append
	 */
	inline void push_back(T&& value) {
		emplace_back(forward<T>(value));
	}

	/*! \brief Creats element at the specified location
	 * \param pos position
	 * \param args Arguments to create the value
	 */
	template<typename... ARGS>
	inline void emplace(size_t pos, ARGS&&... args) {
		if (pos == _capacity) {
			expand();
		}

		if (pos <= _size) {
			for (size_t i = _size; i > pos; --i) {
				_element[i] = move(_element[i - 1]);
			}
			_element[pos] = move(T(forward<ARGS>(args)...));
			_size++;
		}
	}

	/*! \brief Inserts element at the specified location
	 * \param pos position
	 * \param value value
	 */
	inline void insert(size_t pos, const T& value) {
		emplace(pos, value);
	}

	/*! \brief Inserts element at the specified location
	 * \param pos position
	 * \param value value
	 */
	inline void insert(size_t pos, T&& value) {
		emplace(pos, forward<T>(value));
	}

	/*! \brief Remove the last element
	 * \return last element
	 */
	inline T pop_back() {
		return 0 == _size ? _invalid : _element[--_size];
	}

	/*! \brief Remove element at the specified location
	 * \param pos position
	 * \return value or T() if position out of bounds
	 */
	inline T remove(size_t pos) {
		if (pos < _size) {
			T r = _element[pos];
			for (size_t i = pos; i < _size - 1; i++) {
				_element[i] = move(_element[i + 1]);
			}
			_size--;
			return r;
		} else {
			return _invalid;
		}
	}

	/*! \brief Access Element
	 * \param i index
	 */
	inline T & operator[](int i) {
		return _element[i];
	}

	/*! \brief Assignment
	 * \param other
	 */
	Vector<T>& operator=(const Vector<T>& other) {
		// No self assignment
		if (this != &other) {
			if (_capacity != 0) {
				delete _element;
			}
			_size = other._size;
			_capacity = other._capacity;
			if (other.entires > 0) {
				_element = new T[other._capacity];
				for (size_t i = 0; i < _size; ++i) {
					_element[i] = other._element[i];
				}
			} else {
				_element = nullptr;
			}
		}
		return *this;
	}

	/*! \brief Concatenate vector
	 * \param other vector
	 * \return reference to vector
	 */
	Vector<T> & operator+=(const Vector<T>& other) {
		reserve(_size + other._size);
		for (size_t i = 0; i < other._size; ++i) {
			push_back(i);
		}
		return *this;
	}

	/*! \brief Concatenate element
	 * \param element element
	 * \return reference to vector
	 */
	Vector<T> & operator+=(const T& element) {
		push_back(element);
		return *this;
	}

	/*! \brief Concatenate two vectors
	 * \param other vector
	 * \return new vector with all elements of this and other vector
	 */
	Vector<T> operator+(const Vector<T>& other) const {
		Vector<T> r = *this;
		r += other;
		return r;
	}

	/*! \brief Concatenate element
	 * \param element element to Concatenate
	 * \return new vector with all elements of this vector and element
	 */
	Vector<T> operator+(const T& element) const {
		Vector<T> r = *this;
		r += element;
		return r;
	}

	/*! \brief Vector iterator
	 */
	class Iterator {
		T* i;

	 public:  //NOLINT
		explicit Iterator(T* p) : i(p) {}

		Iterator& operator++() {
			i++;
			return *this;
		}

		Iterator& operator--() {
			i--;
			return *this;
		}

		T& operator*() {
			return *i;
		}

		bool operator==(const Vector<T>::Iterator& other) const {
			return *i == *other.i;
		}

		bool operator!=(const Vector<T>::Iterator& other) const {
			return *i != *other.i;
		}
	};

	inline Vector<T>::Iterator begin() const {
		return Vector<T>::Iterator(&_element[0]);
	}

	inline Vector<T>::Iterator end() const {
		return Vector<T>::Iterator(&_element[_size]);
	}
};


/*! \brief Print contents of a vector
 *
 *  \param bs Target Bufferstream
 *  \param val Vector to be printed
 *  \return Reference to BufferStream os; allows operator chaining.
 */
template<typename T>
static inline BufferStream& operator<<(BufferStream& bs, Vector<T> & val) {
	bs << "{ ";
	bool p = false;
	for (const auto & v : val) {
		if (p)
			bs << ", ";
		p = true;
		bs << v;
	}
	return bs << '}';
}
