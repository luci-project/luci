#pragma once

#include <cassert>
#include <cstdlib>
#include <cstring>

#include "internal/comparison.hpp"
#include "internal/elements.hpp"
#include "internal/hasher.hpp"
#include "internal/keyvalue.hpp"
#include "../utility.hpp"
#include "pair.hpp"
#include "optional.hpp"

/*! \brief Hash set
 * \tparam T type for container
 * \tparam H structure with hash functions (operator())
 * \tparam C structure with comparison functions (equal())
 * \tparam L percentage of hash buckets (compared to capacity)
 */
template<typename T, typename H = Hasher, typename C = Comparison, size_t L = 150>
class HashSet : protected Elements<T> {
 protected:
	using typename Elements<T>::_capacity;
	using typename Elements<T>::_next;
	using typename Elements<T>::_count;
	using typename Elements<T>::_node;

	uint32_t _bucket_capacity = 0;

	uint32_t * _bucket = nullptr;

 public:
	using typename Elements<T>::Node;

	explicit HashSet(size_t capacity = 1024) {
		resize(capacity);
	}

	HashSet(const Elements<T>& elements) : Elements<T>(elements) {
		rehash();
	}

	HashSet(Elements<T>&& elements) : Elements<T>(move(elements)) {
		rehash();
	}

	template<typename X>
	explicit HashSet(const X& container) {
		for (const auto & c : container)
			insert(c);
	}

	virtual ~HashSet() {
		free(_bucket);
	}

	/*! \brief hash set iterator
	 */
	class Iterator {
		friend class HashSet<T>;
		HashSet<T> &ref;
		uint32_t i;

		explicit Iterator(HashSet<T> &ref, uint32_t p) : ref(ref), i(p) {}

	 public:
		Iterator& operator++() {
			do {
				i++;
			} while (!ref._node[i].hash.active && i < ref._next);
			return *this;
		}

		Iterator& operator--() {
			do {
				i--;
			} while (!ref._node[i].hash.active && i > 1);
			return *this;
		}

		T& operator*() & {
			assert(ref._node[i].hash.active);
			return ref._node[i].data;
		}

		T&& operator*() && {
			assert(ref._node[i].hash.active);
			return move(ref._node[i].data);
		}

		T* operator->() {
			assert(ref._node[i].hash.active);
			return &(ref._node[i].data);
		}

		bool operator==(const Iterator& other) const {
			return &ref == &other.ref && i == other.i;
		}

		bool operator==(const T& other) const {
			return ref.element[i].data == other;
		}

		bool operator!=(const Iterator& other) const {
			return &ref != &other.ref || i != other.i;
		}

		bool operator!=(const T& other) const {
			return ref.element[i].data != other;
		}

		operator bool() const {
			return i != ref._next;
		}
	};

	inline Iterator begin() {
		return Iterator(*this, 1);
	}

	inline Iterator end() {
		return Iterator(*this, Elements<T>::_next);
	}

	template<typename... ARGS>
	Pair<Iterator,bool> emplace(ARGS&&... args) {
		// Increase capacity (if required)
		if (Elements<T>::_next >= Elements<T>::_capacity) {
			if (!resize(Elements<T>::_count * 2))
				return { end(), false };
		}

		// Create local element
		auto & next = Elements<T>::_node[Elements<T>::_next];
		next.data = move(T(forward<ARGS>(args)...));
		uint32_t * b = bucket(next.hash.temp = hash(next.data));

		// Check if already in set
		auto i = find(b, next.data);
		if (i != end())
			return { i, false };

		// Insert
		next.hash.active = true;
		next.hash.next = *b;
		next.hash.prev = 0;
		if (*b != 0) {
			assert(Elements<T>::_node[*b].active);
			assert(Elements<T>::_node[*b].prev == 0);
			Elements<T>::_node[*b].prev = Elements<T>::_next;
		}
		*b = Elements<T>::_next;
		return { Iterator(*this, Elements<T>::_next++), true };
	}

	/*! \brief Insert element */
	Pair<Iterator,bool> insert(const T &value) {
		// Increase capacity (if required)
		if (Elements<T>::_next >= Elements<T>::_capacity) {
			if (!resize(Elements<T>::_count * 2))
				return { end(), false };
		}

		// Insert (or find)
		auto i = insert(Elements<T>::_next, value);
		if (i.second)
			Elements<T>::_next++;
		return i;
	}

	/*
	Pair<Iterator,bool> insert(const Iterator & position, const T & value) {
		auto & e = Elements<T>::_node[position.i];
		if (e.hash.active)
			return C::equal(e.data, value) ? Pair<Iterator,bool>(position, false) : insert(value);
		else
			return insert(position.i, value);
	}
	*/

	Iterator find(const T& value) {
		return find(bucket(hash(value)), value);
	}

	/*! \brief Erase elements */
	Optional<T> erase(const Iterator & position) {
		assert(position.i >= 1);
		auto & e = Elements<T>::_node[position.i];
		if (e.hash.active && position.i < Elements<T>::_next) {
			e.hash.active = 0;

			uint32_t * b = bucket(e.hash.temp);
			assert(*b != 0);

			auto next = e.hash.next;
			auto prev = e.hash.prev;

			if (*b == position.i) {
				assert(prev == 0);
				*b = next;
				if (next != 0) {
					assert(Elements<T>::_node[next].hash.prev == position.i);
					Elements<T>::_node[next].hash.prev = 0;
				}
			} else {
				if (next != 0) {
					assert(Elements<T>::_node[next].hash.active);
					assert(Elements<T>::_node[next].hash.prev == position.i);
					Elements<T>::_node[next].hash.prev = prev;
				}
				if (prev != 0) {
					assert(Elements<T>::_node[prev].hash.active);
					assert(Elements<T>::_node[prev].hash.next == position.i);
					Elements<T>::_node[prev].hash.next = next;
				}
			}
			return Optional<T>(move(e.data));
		} else {
			return Optional<T>();
		}
	}

	Optional<T> erase(const T & value) {
		return erase(find(value));
		/*
		uint32_t * bucket = hash(value);
		if (*bucket != 0) {
			if (C::equal(Elements<T>::_node[*bucket], value)) {
				size_t n = *bucket;
				*bucket = Elements<T>::_node[n].hash.next;
				Elements<T>::_node[n].hash.active = false;
				return { Elements<T>::_node[n] };
			} else {
				for (size_t i = *bucket; i != 0; i = Elements<T>::_node[i].hash.next) {
					assert(i < Elements<T>::_next);
					assert(Elements<T>::_node[i].hash.active);
					size_t n = Elements<T>::_node[i].hash.next;
					if (n != 0 && C::equal(Elements<T>::_node[n].data, value)) {
						assert(Elements<T>::_node[n].hash.active);
						Elements<T>::_node[i].hash.next = Elements<T>::_node[n].hash.next;
						Elements<T>::_node[n].hash.active = false;
						return { Elements<T>::_node[n] };
					}
				}
			}
		}
		return {};
		*/
	}

	void rehash() {
		reorganize(true);
	}

	bool resize(size_t capacity) {
		if (capacity <= Elements<T>::_count || capacity > UINT32_MAX)
			return false;

		// reorder node slots
		if (Elements<T>::_count + 1 < Elements<T>::_next) {
			size_t j = Elements<T>::_next - 1;
			for (size_t i = 1; i < Elements<T>::_count; i++) {
				if (!Elements<T>::_node[i].hash.active) {
					for (; !Elements<T>::_node[j].hash.active; --j)
						assert(j > i);
					Elements<T>::_node[i].hash.active = true;
					Elements<T>::_node[i].data = move(Elements<T>::_node[j].data);
					Elements<T>::_node[j].hash.active = false;
				}
			}
			assert(j <= Elements<T>::_count);
			Elements<T>::_next = Elements<T>::_count + 1;
		}

		// Resize
		if (capacity != Elements<T>::_capacity) {
			// node slots
			if (!Elements<T>::resize(capacity)) {
				reorganize();
				return false;
			}

			// hash slots
			auto bucket_ptr = _bucket;
			size_t bucket_cap = capacity * L / 100;
			if (bucket_cap >= UINT32_MAX)
				bucket_cap = UINT32_MAX - 1;
			if ((_bucket = reinterpret_cast<uint32_t *>(calloc(sizeof(uint32_t), bucket_cap))) == nullptr) {
				reorganize();
				return false;
			}
			_bucket_capacity = bucket_cap;
			free(bucket_ptr);
		}

		reorganize();
		return true;
	}

	/*! \brief Test whether container is empty */
	inline bool empty() const {
		return Elements<T>::_count == 0;
	}

	/*! Return container size */
	inline size_t size() const {
		return Elements<T>::_count;
	}

	size_t bucket_count() const {
		size_t i = 0;
		for (size_t c = 0; c < _bucket_capacity; c++)
			if (_bucket[c] != 0)
				i++;
		return i;
	}

	inline size_t bucket_size() const {
		return _bucket_capacity;
	}

	/*! Clear contents */
	void clear() const {
		Elements<T>::_next = 1;
		Elements<T>::_count = 0;

		memset(_bucket, 0, sizeof(uint32_t) * _bucket_capacity);
		memset(_bucket, 0, sizeof(Node) * Elements<T>::_capacity);
	}

 private:
	inline uint32_t hash(const T & value) const {
		H h;
		return h(value);
	}

	inline uint32_t * bucket(uint32_t h) const {
		return _bucket + (h % _bucket_capacity);
	}

	void reorganize(bool rehash = false) {
		for (size_t i = 1; i <= Elements<T>::_count; i++) {
			if (Elements<T>::_node[i].hash.active) {
				if (rehash)
					Elements<T>::_node[i].hash.temp = hash(Elements<T>::_node[i].data);

				uint32_t * b = bucket(Elements<T>::_node[i].hash.temp);
				Elements<T>::_node[i].hash.prev = 0;
				if ((Elements<T>::_node[i].hash.next = *b) != 0) {
					assert(Elements<T>::_node[*b].hash.active);
					assert(Elements<T>::_node[*b].hash.prev = 0);
					Elements<T>::_node[*b].hash.prev = i;
				}
				*b = i;
			}
		}
	}

	inline Iterator find(uint32_t * bucket, const T &value) {
		// Find
		for (size_t i = *bucket; i != 0; i = Elements<T>::_node[i].hash.next) {
			assert(i < Elements<T>::_next);
			assert(Elements<T>::_node[i].hash.active);
			if (C::equal(Elements<T>::_node[i].data, value))
				return Iterator(*this, i);
		}
		// End (not found)
		return end();
	}

	Pair<Iterator,bool> insert(uint32_t pos, const T &value) {
		uint32_t h = hash(value);
		uint32_t * b = bucket(h);

		// Check if already in set
		auto i = find(b, value);
		if ( i != end())
			return { i, false };

		// Insert at position
		Elements<T>::_node[pos].hash.active = true;
		Elements<T>::_node[pos].hash.prev = 0;
		Elements<T>::_node[pos].hash.temp = h;
		if ((Elements<T>::_node[pos].hash.next = *b) != 0) {
			assert(Elements<T>::_node[*b].hash.active);
			assert(Elements<T>::_node[*b].hash.prev == 0);
			Elements<T>::_node[*b].hash.prev = pos;
		}

		*b = pos;
		Elements<T>::_node[pos].data = value;
		return { Iterator(*this, pos), true };
	}
};


template<typename K, typename V, typename H = Hasher, typename C = Comparison, size_t L = 150>
class HashMap : protected HashSet<KeyValue<K,V>, H, C, L> {
	using Base = HashSet<KeyValue<K,V>, H, C, L>;

 public:
	using typename Base::Iterator;
	using typename Base::begin;
	using typename Base::end;
	using typename Base::resize;
	using typename Base::rehash;
	using typename Base::empty;
	using typename Base::size;
	using typename Base::bucket_size;
	using typename Base::bucket_count;
	using typename Base::clear;

	/*! \brief Insert element */
	inline Pair<Iterator,bool> insert(const K& key, const V& value) {
		return Base::insert(KeyValue<K,V>(key, value));
	}

	inline Pair<Iterator,bool> insert(K&& key, V&& value) {
		return Base::insert(KeyValue<K,V>(move(key), move(value)));
	}

	inline Pair<Iterator,bool> insert(const Iterator & position, const K& key, const V& value) {
		return Base::insert(position.i, KeyValue<K,V>(key, value));
	}

	inline Pair<Iterator,bool> insert(const Iterator & position, K&& key, V&& value) {
		return Base::insert(position.i, KeyValue<K,V>(move(key), move(value)));
	}

	inline Iterator find(const K& key) {
		return Base::find(KeyValue<K,V>(key));
	}

	inline Iterator find(K&& key) {
		return Base::find(KeyValue<K,V>(move(key)));
	}

	Optional<V> erase(const Iterator & position) {
		auto i = Base::erase(position);
		return i ? Optional<V>(move(i.value().value)) : Optional<V>();
	}

	Optional<V> erase(const K& key) {
		auto i = Base::erase(KeyValue<K,V>(key));
		return i ? Optional<V>(move(i.value().value)) : Optional<V>();
	}

	Optional<V> erase(K&& key) {
		auto i = Base::erase(KeyValue<K,V>(move(key)));
		return i ? Optional<V>(move(i.value().value)) : Optional<V>();
	}

	Optional<V> at(const K& key) {
		auto i = Base::find(key);
		return i ? Optional<V>(move(i.value().value)) : Optional<V>();
	}

	Optional<V> at(K&& key) {
		auto i = Base::find(move(key));
		return i ? Optional<V>(move(i.value().value)) : Optional<V>();
	}

	V & operator[](const K& key) {
		return (*(Base::insert(key).first)).value;
	}

	V & operator[](K&& key) {
		return (*(Base::insert(move(key).first))).value;
	}
};
