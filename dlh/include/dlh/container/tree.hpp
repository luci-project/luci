#pragma once

#include <cassert>
#include <cstdlib>
#include <cstring>

#include "internal/comparison.hpp"
#include "internal/elements.hpp"
#include "internal/keyvalue.hpp"
#include "../utility.hpp"
#include "pair.hpp"
#include "optional.hpp"

template<typename T, typename C = Comparison>
class TreeSet : protected Elements<T> {
 protected:
	uint32_t _root;

 public:
	using typename Elements<T>::Node;

	explicit TreeSet(size_t capacity = 1024) : _root(0) {
		bool r = Elements<T>::resize(capacity);
		assert(r);
	}

/*
	TreeSet(const Elements<T>& elements) : Elements<T>(elements) {
		rehash();
	}

	TreeSet(Elements<T>&& elements) : Elements<T>(move(elements)) {
		rehash();
	}
*/

	template<typename C>
	explicit TreeSet(const C& container) {
		for (const auto & c : container)
			insert(c);
	}

	virtual ~TreeSet() {}

	/*! \brief hash set iterator
	 */
	class Iterator {
		friend class TreeSet<T>;
		TreeSet<T> &ref;
		uint32_t i;

		explicit Iterator(TreeSet<T> &ref, uint32_t p) : ref(ref), i(p) {}

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

		T& operator*() {
			assert(ref._node[i].hash.active);
			return ref._node[i].data;
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

	template<typename... ARGS>
	Pair<Iterator,bool> emplace(ARGS&&... args) {
		// Increase capacity (if required)
		if (Elements<T>::_next >= Elements<T>::_capacity) {
			if (!resize(Elements<T>::_count * 2))
				return { end(), false };
		}

		// Create local element
		Elements<T>::_node[Elements<T>::_next].data = move(T(forward<ARGS>(args)...));

		int c = 0;
		uint32_t i = _root;
		if (contains(Elements<T>::_node[Elements<T>::_next].data, i, c))
			return Pair<Iterator,bool>(Iterator(*this, i), false);
		else
			return insert(i, Elements<T>::_next++, c);
	}

	/*! \brief Insert element */
	Pair<Iterator,bool> insert(const T &value) {
		// Increase capacity (if required)
		if (Elements<T>::_next >= Elements<T>::_capacity)
			if (!resize(Elements<T>::_count * 2))
				return { end(), false };

		int c = 0;
		uint32_t i = _root;
		if (contains(value, i, c)) {
			return Pair<Iterator,bool>(Iterator(*this, i), false);
		} else {
			Elements<T>::_node[Elements<T>::_next].data = value;
			return insert(i, Elements<T>::_next++, c);
		}
	}

	/*! \brief Insert element at node
	 * \note No safety check - position must be the correct parent node!
	 */
	/*
	Pair<Iterator,bool> insert(const Iterator & position, const T & value) {
		// Increase capacity (if required)
		if (Elements<T>::_next >= Elements<T>::_capacity) {
			if (resize(Elements<T>::_count * 2))
				// Ignore iterator, since it might be invalid after resize
				return insert(value);
			else
				return { end(), false };
		} else {
			int c = 0;
			if (position.i != 0) {
				assert(Elements<T>::_node[position.i].tree.active);
				if ((c = C.compare(Elements<T>::_node[position.i].data, value)) == 0)
					return { position, false };
			}
			Elements<T>::_node[Elements<T>::_next].data = value;
			return insert(position.i, Elements<T>::_next++, c);
		}
	}
	*/

	Iterator find(const T& value) {
		uint32_t r = _root;
		return contains(value, r) ? Iterator(*this, r) : end();
	}


	/*! \brief Iterator to the lowest element in this set */
	inline Iterator begin() {
		return Iterator(*this, _min);
	}

	/*! \brief Iterator to the lowest element in this set (alias) */
	inline Iterator lowest() {
		uint32_t i = _root;
		if (i != 0)
			while (Elements<T>::_node[i]->left != 0)
				i = Elements<T>::_node[i]->left;
		return Iterator(*this, i);
	}

	/*! \brief Iterator to the greatest element in this set less than the given element */
	Iterator lower(const T& value) {

	}

	/*! \brief Iterator to the greatest element in this set less than or equal to the given element */
	Iterator floor(const T& value) {

	}

	/*! \brief Iterator to the smallest element in this set greater than or equal to the given element */
	Iterator ceil(const T& value) {

	}

	/*! \brief Iterator to the smallest element in this set greater than the given element */
	Iterator higher(const T& value) {

	}

	/*! \brief Iterator to the highest element in this set */
	inline Iterator highest() {
		uint32_t i = _root;
		if (i != 0)
			while (Elements<T>::_node[i]->right != 0)
				i = Elements<T>::_node[i]->right;
		return Iterator(*this, i);
	}

	/*! \brief Iterator refering to the past-the-end element in this set */
	inline Iterator end() {
		return Iterator(*this, 0);
	}

	bool contains(const T& value) const {
		uint32_t r = _root;
		return contains(value, r);
	}

	/*! \brief Erase elements */
	Optional<T> erase(const Iterator & position) {
		assert(position.i >= 1);
		if (position.i >= 1 && position.i < Elements<T>::_capacity && Elements<T>::_node[position.i].tree.active) {
			auto & e = Elements<T>::_node[position.i];
			e.tree.active = false;

			uint32_t child = 0
			if (e.tree.left == 0)
				child = e.tree.right;
			else if (e.tree.right == 0)
				child = e.tree.left;
			else {
				child = e.tree.left;
				while (Elements<T>::_node[child].tree.right != 0)
					child = Elements<T>::_node[child].tree.right;

				auto & c = Elements<T>::_node[child];
				c.tree.right = e.tree.right;
				Elements<T>::_node[e.tree.right].tree.parent = child;

				c.tree.left = e.tree.left;
				Elements<T>::_node[e.tree.left].tree.parent = child;

				Elements<T>::_node[c.tree.parent].tree.right = 0;
			}

			Elements<T>::_node[child].tree.parent = e.parent;
			auto & p = Elements<T>::_node[e.parent];
			if (p.tree.right == position.i)
				p.tree.right = child;
			else if (p.tree.left == position.i)
				p.tree.left = child;
			else
				assert(false);

			// rebalance
	
			return { e.data };
		}
		return {};
	}

	Optional<T> erase(const T & value) {
		uint32_t * bucket = hash(value);
		if (*bucket != 0) {
			if (equal(Elements<T>::_node[*bucket], value)) {
				size_t n = *bucket;
				*bucket = Elements<T>::_node[n].hash.next;
				Elements<T>::_node[n].hash.active = false;
				return { Elements<T>::_node[n] };
			} else {
				for (size_t i = *bucket; i != 0; i = Elements<T>::_node[i].hash.next) {
					assert(i < Elements<T>::_next);
					assert(Elements<T>::_node[i].hash.active);
					size_t n = Elements<T>::_node[i].hash.next;
					if (n != 0 && equal(Elements<T>::_node[n].data, value)) {
						assert(Elements<T>::_node[n].hash.active);
						Elements<T>::_node[i].hash.next = Elements<T>::_node[n].hash.next;
						Elements<T>::_node[n].hash.active = false;
						return { Elements<T>::_node[n] };
					}
				}
			}
		}
		return {};
	}

	void rebalance() {
		for (size_t i = 1; i <= Elements<T>::_count; i++) {
			if (Elements<T>::_node[i].hash.active) {
				uint32_t * bucket = hash(Elements<T>::_node[i].data);
				Elements<T>::_node[i].hash.next = *bucket;
				*bucket = i;
			}
		}
	}

	bool resize(size_t capacity) {
		if (capacity <= Elements<T>::_count || capacity > UINT32_MAX)
			return false;

		// reorder node slots
		if (Elements<T>::_count + 1 < Elements<T>::_next) {
			size_t j = Elements<T>::_next - 1;
			for (size_t i = 1; i < Elements<T>::_next; i++) {
				if (!Elements<T>::_node[i].tree.active) {
					for (; !Elements<T>::_node[j].tree.active; --j)
						assert(j > i);
					Elements<T>::_node[i] = move(Elements<T>::_node[j]);

					auto left =	Elements<T>::_node[i].tree.left;
					if (left != 0) {
						assert(Elements<T>::_node[left].tree.parent == j);
						Elements<T>::_node[left].tree.parent = i;
					}

					auto right = Elements<T>::_node[i].tree.right;
					if (right != 0) {
						assert(Elements<T>::_node[right].tree.parent == j);
						Elements<T>::_node[right].tree.parent = i;
					}

					auto parent = Elements<T>::_node[i].tree.parent;
					if (parent == 0) {
						assert(_root == j);
						_root = i;
					} else {
						if (Elements<T>::_node[parent].tree.right == j)
							Elements<T>::_node[parent].tree.right = i;
						else if (Elements<T>::_node[parent].tree.left == j)
							Elements<T>::_node[parent].tree.left = i;
						else
							assert(false);
					}

					Elements<T>::_node[j].tree.active = false;
				}
			}
			assert(j <= Elements<T>::_count);
			Elements<T>::_next = Elements<T>::_count + 1;
		}

		// Resize
		return capacity != Elements<T>::_capacity ? Elements<T>::resize(capacity) : true;
	}

	/*! \brief Test whether container is empty */
	bool empty() const {
		return Elements<T>::_count == 0;
	}

	/*! Return container size */
	size_t size() const {
		return Elements<T>::_count;
	}

	/*! Clear contents */
	void clear() const {
		Elements<T>::_next = 1;
		Elements<T>::_count = 0;

		memset(_bucket, 0, sizeof(uint32_t) * _bucket_capacity);
		memset(_bucket, 0, sizeof(Node) * Elements<T>::_capacity);
	}

 private:
	inline Iterator find(uint32_t * bucket, const T &value) {
		// Find
		for (size_t i = *bucket; i != 0; i = Elements<T>::_node[i].hash.next) {
			assert(i < Elements<T>::_next);
			assert(Elements<T>::_node[i].hash.active);
			if (equal(Elements<T>::_node[i].data, value))
				return Iterator(*this, i);
		}
		// End (not found)
		return end();
	}

	/*! \brief Insert helper
	 * \param parent index of parent node or 0 if unknown (or root)
	 * \param element index of element to insert
	 * \param c comparison result of element with parent
	 */
	Pair<Iterator,bool> insert(uint32_t parent, uint32_t element, int c) {
		assert(element != 0);
		auto & e = Elements<T>::_node[element];
		if (_root == 0) {
			assert(parent == 0);
			assert(Elements<T>::_count == 1)
			_root = element;
		} else {
			if ((parent == 0 && contains(e.data, parent, c)) || c == 0) {
				assert(Elements<T>::_node[parent].tree.active);
				return Pair<Iterator,bool>(Iterator(*this, parent), false);
			} else if (c < 0) {
				assert(Elements<T>::_node[parent].tree.right == 0);
				Elements<T>::_node[parent].tree.right = element;
			} else /* if (c > 0) */ {
				assert(Elements<T>::_node[parent].tree.left == 0);
				Elements<T>::_node[parent].tree.left = element;
			}
		}
		e.tree.active = true;
		e.tree.left = 0;
		e.tree.right = 0;
		e.tree.parent = parent;
		return Pair<Iterator,bool>(Iterator(*this, element), true);
	}

	/*! \brief Check if set contains node
	 * \param value Value to search
	 * \param i root, will contain index of nearest node
	 * \param c reference to last comparison result
	 * \return true if found, false otherwise
	 */
	bool contains(const T& value, uint32_t & i, int & c = 0) const {
		if (i != 0)
			while (true) {
				auto & e = Elements<T>::_node[i];
				c = C.compare(e.data, value);
				if (c == 0) {
					return true;
				} else if (c < 0) {
				 	if (e.left != 0)
						i = e.left;
					else
						break;
				} else /* if (c > 0) */ {
				 	if (e.right != 0)
						i = e.right;
					else
						break;
				}
			}

		return false;
	}
};

/*
template<typename K, typename V, typename H = Hash, size_t LOADPERCENT = 50>
class HashMap : protected HashSet<KeyValue<K,V>, H, LOADPERCENT> {
	using Base = HashSet<KeyValue<K,V>, H, LOADPERCENT>;

 public:
	using typename Base::Iterator;
	using typename Base::begin;
	using typename Base::end;
	using typename Base::resize;
	using typename Base::rehash;
	using typename Base::empty;
	using typename Base::size;
	using typename Base::clear;

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
*/
