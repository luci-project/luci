#pragma once

#include "../utility.hpp"

template<class T>
class Optional {
	const bool _assigned;
	union {
		bool _used;
		T _value;
	};

 public:
	Optional() : _assigned(false), _used(false) {}

	Optional(const T& value) : _assigned(true), _value(value) {}

	Optional(T&& value) : _assigned(true), _value(move(value)) {}

	template <class... ARGS>
	Optional(ARGS&&... args) : _assigned(true), _value(move(T(forward<ARGS>(args)...))) {}

	T* operator->() {
		assert(_assigned);
		return &_value;
	}

	T& operator*() & {
		assert(_assigned);
		return _value;
	}

	T&& operator*() && {
		assert(_assigned);
		return move(_value);
	}

	T& value() & {
		assert(_assigned);
		return _value;
	}

	T&& value() && {
		assert(_assigned);
		return move(_value);
	}

	T& value_or(T& alt) & {
		return _assigned ? _value : alt;
	}

	T&& value_or(T&& alt) && {
		return _assigned ? move(_value) : move(alt);
	}

	operator bool() const {
		return _assigned;
	}

	bool has_value() const {
		return _assigned;
	}

	template<typename O>
	bool operator==(const Optional<O>& other) {
		return _assigned == other._assigned || _value == other.value;
	}

	template<typename O>
	bool operator==(const O& other) {
		return _assigned ? _value == other : false;
	}

	template<typename O>
	bool operator!=(const Optional<O>& other) {
		return _assigned != other._assigned || _value != other.value;
	}

	template<typename O>
	bool operator!=(const O& other) {
		return _assigned ? _value != other : true;
	}
};
