#pragma once

#include <cstdint>

#include "keyvalue.hpp"
#include "../pair.hpp"


struct Hasher {
	inline uint32_t operator()(uint64_t v) const {
		union {
			struct {
				uint32_t a, b;
			} half;
			uint64_t full;
		};
		full = v;
		return half.a ^ half.b;
	}

	inline uint32_t operator()(int64_t v) const {
		return operator()(static_cast<uint64_t>(v));
	}

	inline uint32_t operator()(void * v) const {
		return operator()(reinterpret_cast<uint64_t>(v));
	}

	inline uint32_t operator()(const char * v) const {
		uint_fast32_t h = 5381;
		for (unsigned char c = *v; c != '\0'; c = *++v)
			h = h * 33 + c;
		return h & 0xffffffff;
	}

	template<typename T>
	inline uint32_t operator()(const T& v) const {
		return static_cast<uint32_t>(v);
	}

	template<class OK, class OV>
	inline uint32_t operator()(const KeyValue<OK,OV>& o) {
		return operator()(o.key);
	}

	template<class OF, class OS>
	inline uint32_t operator()(const Pair<OF,OS>& o) {
		return operator()(o.first) ^ operator()(o.second);
	}

};
