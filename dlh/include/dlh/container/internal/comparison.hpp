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


	static inline uint32_t hash(uint64_t v) {
		union {
			struct {
				uint32_t a, b;
			} half;
			uint64_t full;
		};
		full = v;
		return half.a ^ half.b;
	}

	static inline uint32_t hash(int64_t v) {
		return hash(static_cast<uint64_t>(v));
	}

	static inline uint32_t hash(uint32_t v) {
		return v;
	}

	static inline uint32_t hash(int32_t v) {
		return static_cast<uint32_t>(v);
	}

	static inline uint32_t hash(uint16_t v) {
		return static_cast<uint32_t>(v);
	}

	static inline uint32_t hash(int16_t v) {
		return static_cast<uint32_t>(v);
	}

	static inline uint32_t hash(uint8_t v) {
		return static_cast<uint32_t>(v);
	}

	static inline uint32_t hash(int8_t v) {
		return static_cast<uint32_t>(v);
	}

	static inline uint32_t hash(void * v) {
		return hash(reinterpret_cast<uint64_t>(v));
	}

	static inline uint32_t hash(const char * v) {
		uint_fast32_t h = 5381;
		for (unsigned char c = *v; c != '\0'; c = *++v)
			h = h * 33 + c;
		return h & 0xffffffff;
	}

	template<typename T>
	static inline uint32_t hash(const T * v) {
		uint_fast32_t h = 0;
		const unsigned char *c = reinterpret_cast<const unsigned char *>(v);
		for (size_t i = 0; i < sizeof(T); i++)
			h = h * 31 + c[i];
		return h & 0xffffffff;
	}

	template<typename T>
	static inline uint32_t hash(const T& v) {
		return hash(&v);
	}

	template<class OK, class OV>
	static inline uint32_t hash(const KeyValue<OK,OV>& o) {
		return hash(o.key);
	}

	template<class OF, class OS>
	static inline uint32_t hash(const Pair<OF,OS>& o) {
		return hash(o.first) ^ hash(o.second);
	}
};
