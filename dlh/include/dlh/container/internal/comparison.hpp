#pragma once

#include <cstring>

#include "keyvalue.hpp"

struct Comparison {
	template<typename T>
	static inline int compare(const T& a, const T& b) {
		return a == b ? 0 : (a < b ? -1 : 1);
	}

	template<typename K, typename V>
	static inline int compare(const KeyValue<K,V> & a, const KeyValue<K,V> & b) {
		return compare(a.key, b.key);
	}

	static inline int compare(const char * a, const char * b) {
		return strcmp(a, b);
	}

	template<typename T>
	static inline bool equal(const T& a, const T& b) {
		return a == b;
	}

	template<typename K, typename V>
	static inline bool equal(const KeyValue<K,V> & a, const KeyValue<K,V> & b) {
		return equal(a.key, b.key);
	}

	static inline bool equal(const char * a, const char * b) {
		return compare(a, b) == 0;
	}
};
