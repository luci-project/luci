#pragma once

#include <string>
#include <vector>
#include <climits>

#include "auxiliary.hpp"

namespace Utils {

std::vector<const char *> split(char * source, const char delimiter);

std::vector<const char *> file_contents(const char * path);

char * env(const char * name, bool consume = false);

Auxiliary aux(Auxiliary::type type);


template<typename T>
bool parse(T & element, const char * value) { return false; }

template<>
bool parse(unsigned long long & element, const char * value);

template<>
bool parse(long long & element, const char * value);

template<>
bool parse(unsigned & element, const char * value);

template<>
bool parse(int & element, const char * value);

template<>
bool parse(const char * & element, const char * value);

template<>
bool parse(bool & element, const char * value);

template<typename T>
bool parse(std::vector<T> & element, const char * value) {
	T tmp;
	parse<T>(tmp, value);
	element.push_back(tmp);
	return true;
}

template<typename T>
bool is_default(T & element) {
	T x{};
	return x == element;
}

template<>
inline bool is_default(bool & element) {
	return true;
}

template<typename T>
bool is_vector(T & element) {
	return false;
}

template<typename T>
inline bool is_vector(std::vector<T> & element) {
	return true;
}

}
