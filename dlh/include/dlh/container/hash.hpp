#pragma once

#include "../assert.hpp"
#include "../string.hpp"
#include <cstdlib>
#include <cstring>

#include "internal/comparison.hpp"
#include "internal/elements.hpp"
#include "internal/keyvalue.hpp"
#include "../utility.hpp"
#include "pair.hpp"
#include "optional.hpp"

#include <ostream>

/*! \brief Hash set
 * influenced by standard [template/cxx] librarys `unordered_set`
 * \tparam T type for container
 * \tparam C structure with comparison (`bool equal(const T&, const T&)`)
 *           and hash (`uint32_t hash(const T&)`) functions
 * \tparam L percentage of hash buckets (compared to element capacity)
 */
template<typename T, typename C = Comparison, size_t L = 150>
class HashSet : protected Elements<T> {
 protected:
	using typename Elements<T>::_capacity;
	using typename Elements<T>::_next;
	using typename Elements<T>::_count;
	using typename Elements<T>::_node;

	/*! \brief Hash bucket capacity */
	uint32_t _bucket_capacity = 0;

	/*! \brief Pointer to start of hash bucket array */
	uint32_t * _bucket = nullptr;

 public:
	using typename Elements<T>::Node;

	/*! \brief Create new hash set
	 * \return capacity initial capacity
	 */
	explicit HashSet(size_t capacity = 1024) {
		resize(capacity);
	}

	/*! \brief Convert to hash set
	 * \return elements Elements container
	 */
	HashSet(const Elements<T>& elements) : Elements<T>(elements) {
		rehash();
	}

	/*! \brief Convert to hash set
	 * \return elements Elements container
	 */
	HashSet(Elements<T>&& elements) : Elements<T>(move(elements)) {
		rehash();
	}

	/*! \brief Convert to hash set
	 * \param container Elements container
	 * \param capacity_reserve additional space to reserve
	 */
	/*template<typename X>
	explicit HashSet(const X& container, size_t capacity_reserve = 128) {
		resize(container.size() + capacity_reserve);
		for (const auto & c : container)
			insert(c);
	}
*/
	/*! \brief Destructor
	 */
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
			} while (i < ref._next && !ref._node[i].hash.active);
			return *this;
		}

		inline T& operator*() & {
			assert(ref._node[i].hash.active);
			return ref._node[i].data;
		}

		inline T&& operator*() && {
			assert(ref._node[i].hash.active);
			return move(ref._node[i].data);
		}

		inline T* operator->() {
			assert(ref._node[i].hash.active);
			return &(ref._node[i].data);
		}

		inline bool operator==(const Iterator& other) const {
			return &ref == &other.ref && i == other.i;
		}

		inline bool operator==(const T& other) const {
			return ref.element[i].data == other;
		}

		inline bool operator!=(const Iterator& other) const {
			return &ref != &other.ref || i != other.i;
		}

		inline bool operator!=(const T& other) const {
			return ref.element[i].data != other;
		}

		inline operator bool() const {
			return i != ref._next;
		}
	};

	/*! \brief Get iterator to first element
	 * \return iterator to first valid element (if available) or `end()`
	 */
	inline Iterator begin() {
		uint32_t i = 1;
		while (i < Elements<T>::_next && !Elements<T>::_node[i].hash.active)
			i++;
		return Iterator(*this, i);
	}

	/*! \brief Get iterator to end (post-last-element)
	 * \return iterator to end (first invalid element)
	 */
	inline Iterator end() {
		return Iterator(*this, Elements<T>::_next);
	}

	/*! \brief Create new element into set
	 * \param args Arguments to construct element
	 * \return iterator to the new element (`first`) and
	 *         indicator if element was created (`true`) or has already been in the set (`false`)
	 */
	template<typename... ARGS>
	Pair<Iterator,bool> emplace(ARGS&&... args) {
		increase();

		// Create local element (not active yet!)
		auto & next = Elements<T>::_node[Elements<T>::_next];
		next.data = move(T(forward<ARGS>(args)...));
		uint32_t * b = bucket(next.hash.temp = C::hash(next.data));

		// Check if already in set
		uint32_t i = find(b, next.data);
		if (i != Elements<T>::_next)
			return { Iterator(*this, i), false };

		// Insert
		return insert(Elements<T>::_next++, b);
	}

	/*! \brief Insert element into set
	 * \param value new element to be inserted
	 * \return iterator to the inserted element (`first`) and
	 *         indicator (`second`) if element was created (`true`) or has already been in the set (`false`)
	 */
	Pair<Iterator,bool> insert(const T &value) {
		increase();

		// Get Bucket
		uint32_t h = C::hash(value);
		uint32_t * b = bucket(h);

		// Check if already in set
		uint32_t i = find(b, value);
		if ( i != Elements<T>::_next)
			return { Iterator(*this, i), false };

		// Insert at position
		auto & next = Elements<T>::_node[Elements<T>::_next];
		next.hash.temp = h;
		next.data = value;
		return insert(Elements<T>::_next++, b);
	}

	/*! \brief Get iterator to specific element
	 * \param value element
	 * \return iterator to element (if found) or `end()` (if not found)
	 */
	inline Iterator find(const T& value) {
		return Iterator(*this, find(bucket(C::hash(value)), value));
	}

	/*! \brief check if set contains element
	 * \param value element
	 * \return `true` if element is in set
	 */
	inline bool contains(const T& value) const {
		return find(bucket(C::hash(value)), value) != Elements<T>::_next;
	}

	/*! \brief Remove value from set
	 * \param position iterator to element
	 * \return removed value (if valid iterator)
	 */
	Optional<T> erase(const Iterator & position) {
		assert(position.i >= 1);
		auto & e = Elements<T>::_node[position.i];
		if (e.hash.active && position.i < Elements<T>::_next) {
			e.hash.active = 0;

			uint32_t * b = bucket(e.hash.temp);
			assert(*b != 0);

			auto next = e.hash.next;
			auto prev = e.hash.prev;

			if (next != 0) {
				assert(Elements<T>::_node[next].hash.active);
				assert(Elements<T>::_node[next].hash.prev == position.i);
				Elements<T>::_node[next].hash.prev = prev;
			}

			if (*b == position.i) {
				assert(prev == 0);
				*b = next;
			} else if (prev != 0) {
				assert(Elements<T>::_node[prev].hash.active);
				assert(Elements<T>::_node[prev].hash.next == position.i);
				Elements<T>::_node[prev].hash.next = next;
			}

			Elements<T>::_count--;

			return Optional<T>(move(e.data));
		} else {
			return Optional<T>();
		}
	}

	/*! \brief Remove value from set
	 * \param value element to be removed
	 * \return removed value (if found)
	 */
	inline Optional<T> erase(const T & value) {
		return erase(find(value));
	}

	/*! \brief Recalculate hash values
	 */
	inline void rehash() {
		bucketize(false, true);
	}

	/*! \brief Reorganize Elements
	 * Fill gaps emerged from erasing elments
	 */
	inline void reorganize() {
		if (reorder())
			bucketize();
	}

	/*! \brief Resize set capacity
	 * \param capacity new capacity (has to be equal or greater than `size()`)
	 * \return `true` if resize was successfully, `false` otherwise
	 */
	bool resize(size_t capacity) {
		if (capacity <= Elements<T>::_count || capacity > UINT32_MAX)
			return false;

		// reorder node slots
		bool need_bucketize = reorder();
		bool buckets_zeroed = false;

		bool s = capacity == Elements<T>::_capacity;
		// Resize element container
		if (!s && Elements<T>::resize(capacity)) {
			// resize hash buckets
			size_t bucket_cap = capacity * L / 100;
			if (bucket_cap >= UINT32_MAX)
				bucket_cap = UINT32_MAX - 1;

			auto bucket_ptr = reinterpret_cast<uint32_t *>(calloc(sizeof(uint32_t), bucket_cap));
			if (bucket_ptr != nullptr) {
				free(_bucket);
				_bucket = bucket_ptr;
				_bucket_capacity = bucket_cap;
				s = buckets_zeroed = need_bucketize = true;
			}

		}

		// Bucketize (if either reordering or resizing of hash buckets was successful)
		if (need_bucketize)
			bucketize(buckets_zeroed);

		return s;
	}

	/*! \brief Test whether container is empty
	 * \return true if set is empty
	 */
	inline bool empty() const {
		return Elements<T>::_count == 0;
	}

	/*! \brief Element count
	 * \return Number of (unique) elements in set
	 */
	inline size_t size() const {
		return Elements<T>::_count;
	}

	/*! brief Used buckets
	 * \return Number of non-empty hash buckets (<= `size()`)
	 */
	size_t bucket_count() const {
		size_t i = 0;
		for (size_t c = 0; c < _bucket_capacity; c++)
			if (_bucket[c] != 0)
				i++;
		return i;
	}

	/*! \brief Available buckets
	 * \return Number of available buckets
	 */
	inline size_t bucket_size() const {
		return _bucket_capacity;
	}

	/*! \brief Clear all elements in set */
	void clear() {
		Elements<T>::_next = 1;
		Elements<T>::_count = 0;

		memset(_bucket, 0, sizeof(uint32_t) * _bucket_capacity);
		memset(_bucket, 0, sizeof(Node) * Elements<T>::_capacity);
	}

	std::ostream & dot(std::ostream & out) const {
		out << "digraph HashSet {\n"
		    << "	rankdir=LR;\n"
		    << "	bucket\n";
		for (size_t i = 1; i < Elements<T>::_next; i++) {
			auto & e = Elements<T>::_node[i];
			if (e.hash.active)
				out << "	e" << i << "[shape=box label=\"" << e.data << "\" xlabel=\"" << e.hash.temp << "\"];\n";
		}
		out << '\n';
		for (size_t i = 0; i < _bucket_capacity; i++)
			if (_bucket[i] != 0)
				out << "	bucket -> e" << _bucket[i] << " [label=\"" << i << " / " << _bucket_capacity << "\"];\n";
		out << '\n';
		for (size_t i = 1; i < Elements<T>::_next; i++) {
			auto & e = Elements<T>::_node[i];
			if (e.hash.active && e.hash.next != 0)
				out << "	e" << i << " -> e" << e.hash.next << ";\n";
		}
		return out << "}\n";
	}

 private:
	/*! \brief Increase capacity (by reordering or resizing) if required
	 * \return `false` on error
	 */
	inline bool increase() {
		if (Elements<T>::_next >= Elements<T>::_capacity) {
			if (Elements<T>::_count * 2 <= Elements<T>::_capacity)
				reorganize();
			else if (!resize(Elements<T>::_capacity * 2))
				return false;
		}
		return true;
	}

	/*! \brief Get bucket for hash value
	 * \param h hash value
	 * \return pointer to bucket
	 */
	inline uint32_t * bucket(uint32_t h) const {
		return _bucket + (h % _bucket_capacity);
	}

	/*! \brief Reorder elements
	 * \return `true` if elements are in a different order (and have to be `bucketize`d again!)
	 */
	bool reorder() {
		if (Elements<T>::_count + 1 < Elements<T>::_next) {
			size_t j = Elements<T>::_next - 1;
			for (size_t i = 1; i <= Elements<T>::_count; i++) {
				if (!Elements<T>::_node[i].hash.active) {
					for (; !Elements<T>::_node[j].hash.active; --j)
						assert(j > i);
					Elements<T>::_node[i] = move(Elements<T>::_node[j]);
					Elements<T>::_node[j--].hash.active = false;
				}
			}
			Elements<T>::_next = Elements<T>::_count + 1;

			return true;
		} else {
			return false;
		}
	}

	/*! \brief Reorganize buckets
	 * Required if bucket capacity has changed
	 * \param is_zero are the bucket slots already zeroed?
	 * \param rehash calculate hash value (replacing cached value)
	 */
	void bucketize(bool is_zero = false, bool rehash = false) {
		if (!is_zero)
			memset(_bucket, 0, _bucket_capacity * sizeof(uint32_t));

		for (size_t i = 1; i <= Elements<T>::_count; i++) {
			if (Elements<T>::_node[i].hash.active) {
				if (rehash)
					Elements<T>::_node[i].hash.temp = C::hash(Elements<T>::_node[i].data);

				uint32_t * b = bucket(Elements<T>::_node[i].hash.temp);
				assert(*b <= Elements<T>::_count);
				Elements<T>::_node[i].hash.prev = 0;
				if ((Elements<T>::_node[i].hash.next = *b) != 0) {
					assert(Elements<T>::_node[*b].hash.active);
					assert(Elements<T>::_node[*b].hash.prev == 0);
					Elements<T>::_node[*b].hash.prev = i;
				}
				*b = i;
			}
		}
	}

	/*! \brief Find value in bucket (helper)
	 * \param bucket bucket determined by value hash
	 * \param value the value we are looking for
	 * \return index of target value or `Elements<T>::_next` if not found
	 */
	inline uint32_t find(uint32_t * bucket, const T &value) const {
		// Find
		for (uint32_t i = *bucket; i != 0; i = Elements<T>::_node[i].hash.next) {
			assert(Elements<T>::_count < Elements<T>::_next);
			assert(i < Elements<T>::_next);
			assert(Elements<T>::_node[i].hash.active);
			if (C::equal(Elements<T>::_node[i].data, value))
				return i;
		}
		// End (not found)
		return Elements<T>::_next;
	}


	/*! \brief Insert helper
	 * \param element index of element to insert
	 * \param b pointer to target bucket
	 */
	inline Pair<Iterator,bool> insert(uint32_t element, uint32_t * b) {
		Elements<T>::_node[element].hash.active = true;
		Elements<T>::_node[element].hash.prev = 0;

		Elements<T>::_count++;

		// Bucket not empty?
		if ((Elements<T>::_node[element].hash.next = *b) != 0) {
			assert(Elements<T>::_node[*b].hash.active);
			assert(Elements<T>::_node[*b].hash.prev == 0);
			Elements<T>::_node[*b].hash.prev = element;
		}

		// Assign bucket
		*b = element;

		// return Iterator
		return { Iterator(*this, element), true };
	}

};

/*! \brief Hash map
 * \tparam K type for key
 * \tparam V type for value
 * \tparam C structure with comparison (`bool equal(const K&, const K&)`)
 *           and hash (`uint32_t hash(const K&)`) functions
 * \tparam L percentage of hash buckets (compared to element capacity)
 */
template<typename K, typename V, typename C = Comparison, size_t L = 150>
class HashMap : protected HashSet<KeyValue<K,V>, C, L> {
	using Base = HashSet<KeyValue<K,V>, C, L>;

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
	using typename Base::dot;

	/*! \brief Insert element */
	inline Pair<Iterator,bool> insert(const K& key, const V& value) {
		return Base::insert(KeyValue<K,V>(key, value));
	}

	inline Pair<Iterator,bool> insert(K&& key, V&& value) {
		return Base::insert(KeyValue<K,V>(move(key), move(value)));
	}

	inline Iterator find(const K& key) {
		return Base::find(KeyValue<K,V>(key));
	}

	inline Iterator find(K&& key) {
		return Base::find(KeyValue<K,V>(move(key)));
	}

	inline bool contains(const K& key) {
		return Base::contains(KeyValue<K,V>(key));
	}

	inline bool contains(K&& key) {
		return Base::contains(KeyValue<K,V>(move(key)));
	}

	inline Optional<V> erase(const Iterator & position) {
		auto i = Base::erase(position);
		return i ? Optional<V>(move(i.value().value)) : Optional<V>();
	}

	inline Optional<V> erase(const K& key) {
		auto i = Base::erase(KeyValue<K,V>(key));
		return i ? Optional<V>(move(i.value().value)) : Optional<V>();
	}

	inline Optional<V> erase(K&& key) {
		auto i = Base::erase(KeyValue<K,V>(move(key)));
		return i ? Optional<V>(move(i.value().value)) : Optional<V>();
	}

	inline Optional<V> at(const K& key) {
		auto i = Base::find(key);
		return i ? Optional<V>(move(i.value().value)) : Optional<V>();
	}

	inline Optional<V> at(K&& key) {
		auto i = Base::find(move(key));
		return i ? Optional<V>(move(i.value().value)) : Optional<V>();
	}

	inline V & operator[](const K& key) {
		return (*(Base::insert(key).first)).value;
	}

	inline V & operator[](K&& key) {
		return (*(Base::insert(move(key).first))).value;
	}
};
