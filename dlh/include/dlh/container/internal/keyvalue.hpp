#pragma once

#include "../../utility.hpp"
#include "../pair.hpp"

template<class K, class V>
struct KeyValue {
	K key;
	union {
		bool _used;
		V value;
	};

	KeyValue() : key(), value() { }

	KeyValue(const K& key) : key(key), _used(false) { }

	KeyValue(K&& key) : key(move(key)), _used(false) { }

	KeyValue(const K& key, const V& value) : key(key), value(value) { }

	KeyValue(K&& key, V&& value) : key(move(key)), value(move(value)) { }

	template<class OK, class OV>
	KeyValue(const KeyValue<OK, OV>& o) : key(o.key), value(o.value) {}

	template<class OF, class OS>
	KeyValue(const Pair<OF, OS>& o) : key(o.first), value(o.second) {}

	template<class OK, class OV>
	KeyValue& operator=(const KeyValue<OK,OV>& o) {
		key = o.key;
		value = o.value;
		return *this;
	}

	template<class OF, class OS>
	KeyValue& operator=(const Pair<OF,OS>& o) {
		key = o.first;
		value = o.second;
		return *this;
	}

	template<class OK, class OV>
	constexpr bool operator==(const KeyValue<OK,OV>& other) const {
		return key == other.key && value == other.value;
	}

	template<class OF, class OS>
	constexpr bool operator==(const Pair<OF,OS>& other) const {
		return key == other.first && value == other.second;
	}

	template<class OK, class OV>
	constexpr bool operator!=(const KeyValue<OK,OV>& other) const {
		return key != other.key || value != other.value;
	}

	template<class OF, class OS>
	constexpr bool operator!=(const Pair<OF,OS>& other) const {
		return key != other.first || value != other.second;
	}
};
