#pragma once

#include <ostream>
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

	/*! \brief Create new balanced binary search tree
	 * \return capacity initial capacity
	 */
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

	/*! \brief Convert to balanced binary search tree
	 * \return container Elements container
	 */
	template<typename C>
	explicit TreeSet(const C& container) {
		for (const auto & c : container)
			insert(c);
	}

	/*! \brief Destructor
	 */
	virtual ~TreeSet() {}

	/*! \brief binary search tree iterator
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

	/*! \brief Create new element into set
	 * \param args Arguments to construct element
	 * \return iterator to the new element (`first`) and
	 *         indicator if element was created (`true`) or has already been in the set (`false`)
	 */
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

	/*! \brief Insert element into set
	 * \param value new element to be inserted
	 * \return iterator to the inserted element (`first`) and
	 *         indicator (`second`) if element was created (`true`) or has already been in the set (`false`)
	 */
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

	/*! \brief Get iterator to specific element
	 * \param value element
	 * \return iterator to element (if found) or `end()` (if not found)
	 */
	Iterator find(const T& value) {
		uint32_t r = _root;
		return contains(value, r) ? Iterator(*this, r) : end();
	}

	/*! \brief check if set contains element
	 * \param value element
	 * \return `true` if element is in set
	 */
	bool contains(const T& value) const {
		uint32_t r = _root;
		return contains(value, r);
	}

	/*! \brief Iterator to the lowest element in this set
	 * \return Iterator to the lowest element
	 */
	inline Iterator begin() {
		return Iterator(*this, min(_root));
	}

	/*! \brief Get the lowest element in this set
	 * \note alias for `begin()`
	 * \return Iterator to the lowest element
	 */
	inline Iterator lowest() {
		return begin();
	}

	/*! \brief Get the greatest element in this set less than the given element
	 * \param value element
	 * \return Iterator to the greatest element less than the given element
	 */
	Iterator lower(const T& value) {
		uint32_t r = 0;
		uint32_t i = _root;
		while (i != 0) {
			auto & e = Elements<T>::_node[i];
			c = C.compare(e.data, value);
			if (c == 0) {
				if (e.left != 0)
					r = max(e.left);
				break;
			} else if (c < 0) {
				r = i;
				i = e.right;
			} else /* if (c > 0) */ {
				i = e.left;
			}
		}
		return { *this, r };
	}

	/*! \brief Get the greatest element in this set less than or equal to the given element
	 * \param value element
	 * \return Iterator to the greatest element less than or equal to the given element
	 */
	Iterator floor(const T& value) {
		uint32_t r = 0;
		uint32_t i = _root;
		while (i != 0) {
			auto & e = Elements<T>::_node[i];
			c = C.compare(e.data, value);
			if (c == 0) {
				r = i;
				break;
			} else if (c < 0) {
				r = i;
				i = e.right;
			} else /* if (c > 0) */ {
				i = e.left;
			}
		}
		return { *this, r };
	}

	/*! \brief Get the smallest element in this set greater than or equal to the given element
	 * \param value element
	 * \return Iterator to the smallest element greater than or equal to the given element
	 */
	Iterator ceil(const T& value) {
		uint32_t r = 0;
		uint32_t i = _root;
		while (i != 0) {
			auto & e = Elements<T>::_node[i];
			c = C.compare(e.data, value);
			if (c == 0) {
				r = i;
				break;
			} else if (c < 0) {
				i = e.right;
			} else /* if (c > 0) */ {
				r = i;
				i = e.left;
			}
		}
		return { *this, r };
	}

	/*! \brief Get the smallest element in this set greater than the given element
	 * \param value element
	 * \return Iterator to the smallest element greater than the given element
	 */
	Iterator higher(const T& value) {
		uint32_t r = 0;
		uint32_t i = _root;
		while (i != 0) {
			auto & e = Elements<T>::_node[i];
			c = C.compare(e.data, value);
			if (c == 0) {
				if (e.right != 0)
					r = min(e.right);
				break;
			} else if (c < 0) {
				i = e.right;
			} else /* if (c > 0) */ {
				r = i;
				i = e.left;
			}
		}
		return { *this, r };
	}

	/*! \brief Get the highest element in this set
	 * \return Iterator to the highest element
	 */
	inline Iterator highest() {
		return Iterator(*this, max(_root));
	}


	/*! \brief Iterator refering to the past-the-end element in this set */
	inline Iterator end() {
		return Iterator(*this, 0);
	}


	/*! \brief Remove value from set
	 * \param position iterator to element
	 * \return removed value (if valid iterator)
	 */
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
				child = max(e.tree.left);

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
			uint32_t node = child;
			uint32_t parent = e.parent;
			while (parent != 0) {
				auto & n = Elements<T>::_node[node];
				auto & p = Elements<T>::_node[parent];
				uint32_t grandparent = p.parent

				if (p.right == node) {
					if (p.balance < 0) {
					 	p.balance = 0;
						node = parent;
					} else if (p.balance == 0) {
						p.balance = 1;
						break;
					} else if (rotate(p.right, grandparent, true, node)) {
						break;
					}
				} else {
					assert(p.right == node);
					if (p.balance > 0) {
					 	p.balance = 0;
						node = parent;
					} else if (p.balance == 0) {
						p.balance = -1;
						break;
					} else if (rotate(p.left, grandparent, false, node)) {
						break;
					}
				}
				parent = grandparent;
			}

			return { e.data };
		}
		return {};
	}

	/*! \brief Remove value from set
	 * \param value element to be removed
	 * \return removed value (if found)
	 */
	Optional<T> erase(const T & value) {
		return erase(find(value));
	}

	ostream & dot(ostream & out) {
		out << "digraph TreeSet {\n";
		for (size_t i = 1; i < Elements<T>::_next; i++) {
			auto & e = Elements<T>::_node[i];
			if (e.tree.active)
				out << "	e" << i << "[label=\"<b>" << e.data << "</b>\" xlabel=\"" << e.tree.balance << (_root == i ? ", root" : "") << "\"];\n";
		}
		out << '\n';
		for (size_t i = 1; i < Elements<T>::_next; i++) {
			auto & e = Elements<T>::_node[i];
			if (e.tree.active && (e.tree.right != 0 || e.tree.left != 0)) {
				out << "	e" << i << " -> {";
				if (e.tree.left != 0)
					out << " e" << e.tree.left;
				if (e.tree.right != 0)
					out << " e" << e.tree.right;
				out << " };\n";
			}
		}
		return out << "}\n";
	}

	/*! \brief Reorganize Elements
	 * Fill gaps emerged from erasing elments
	 */
	inline void reorganize() {
		reorder();
	}

	/*! \brief Resize set capacity
	 * \param capacity new capacity (has to be equal or greater than `size()`)
	 * \return `true` if resize was successfully, `false` otherwise
	 */
	bool resize(size_t capacity) {
		if (capacity <= Elements<T>::_count || capacity > UINT32_MAX)
			return false;

		// reorder node slots
		reorder();

		// Resize
		return capacity != Elements<T>::_capacity ? Elements<T>::resize(capacity) : true;
	}

	/*! \brief Test whether container is empty
	 * \return true if set is empty
	 */
	bool empty() const {
		return Elements<T>::_count == 0;
	}

	/*! \brief Element count
	 * \return Number of (unique) elements in set
	 */
	size_t size() const {
		return Elements<T>::_count;
	}

	/*! \brief Clear all elements in set */
	void clear() const {
		Elements<T>::_next = 1;
		Elements<T>::_count = 0;

		memset(_bucket, 0, sizeof(uint32_t) * _bucket_capacity);
		memset(_bucket, 0, sizeof(Node) * Elements<T>::_capacity);
	}

 private:

	/*! \brief Get the lowest element in the subtree
	 * \param i node definining the subtree
	 * \return minimum value of the subtree
	 */
	inline uint32_t min(uint32_t i) {
		if (i != 0)
			while (Elements<T>::_node[i]->left != 0)
				i = Elements<T>::_node[i]->left;
		return i;
	}


	/*! \brief Get the highest element in the subtree
	 * \param i node definining the subtree
	 * \return max value of the subtree
	 */
	inline uint32_t max(uint32_t i) {
		if (i != 0)
			while (Elements<T>::_node[i]->right != 0)
				i = Elements<T>::_node[i]->right;
		return i;
	}

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
		e.tree.balance = 0;
		e.tree.left = 0;
		e.tree.right = 0;
		e.tree.parent = parent;

		// rebalance
		uint32_t node = element;
		while (parent != 0) {
			auto & n = Elements<T>::_node[node];
			auto & p = Elements<T>::_node[parent];

			const int8_t b = p.right == node ? 1 : -1;
			if (b * p.balance < 0) {
			 	p.balance = 0;
				break;
			} else if (b * p.balance > 0) {
				rotate(node, p.parent, p.right == node);
				break;
			} else {
				p.balance = b;
				node = parent;
				parent = p.parent;
			}
		}

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
				 	if (e.right != 0)
						i = e.right;
					else
						break;
				} else /* if (c > 0) */ {
				 	if (e.left != 0)
						i = e.left;
					else
						break;
				}
			}

		return false;
	}

	/*! \brief Reorder elements
	 * \return `true` if element positions have changed
	 */
	bool reorder() {
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
			return true;
		} else {
			return false;
		}
	}


	/*! \brief Helper to perform rotation
	 * \param node Node to rotate
	 * \param grandparent nodes parents parent node
	 * \param left `true` for left direction, `false` for right
	 * \param parent reference which will contain the new subtree root node
	 * \return `true` if there was no height change
	 */
	bool rotate(const uint32_t node, const uint32_t grandparent, bool left, uint32_t & parent = 0) {
		assert(node != 0);
		auto & n = Elements<T>::_node[node].tree;
		assert(n.active);

		uint8_t b = n.balance;
		parent = (left && b < 0) || (!left && b > 0) ? rotate2(node, left) : rotate1(node, left);

		assert(parent != 0);
		auto & p = Elements<T>::_node[parent].tree;
		assert(p.active);

		if ((p.parent = grandparent) != 0) {
			auto & g = Elements<T>::_node[grandparent].tree;
			assert(g.active);
			if (node == g.left) {
				g.left = parent;
			} else {
				assert(node == g.right);
 				g.right = parent;
			}
		} else {
			_root = parent;
		}

		return b == 0;
	}

	/*! \brief Perform simple rotation
	 * \param node Node to rotate with parent
	 * \param left `true` for left rotation, `false` for right
	 * \return new subtree root node
	 */
	inline uint32_t rotate1(const uint32_t node, const bool left) {
		assert(node != 0);
		auto & n = Elements<T>::_node[node].tree;
		assert(n.active);

		const uint32_t parent = n.parent;
		assert(parent != 0);
		auto & p = Elements<T>::_node[parent];
		assert(p.active);

		if (left) {
			// Rotate left
			if ((p.right = n.left) != 0)
				Elements<T>::_node[p.right].parent = parent;
			n.left = parent;
		} else {
			// Rotate right
			if ((p.left = n.right) != 0)
				Elements<T>::_node[p.left].parent = parent;
			n.right = parent;
		}
		p.parent = node;

		// Adjust balance
		n.balance = n.balance == 0 ? (left ? -1 :  1) : 0;
		p.balance = n.balance == 0 ? (left ?  1 : -1) : 0;

		return node;
	}

	/*! \brief Perform double rotation
	 * \param node Node to rotate with child and parent
	 * \param right_left `true` for right-left rotation, `false` for left-right
	 * \return new subtree root node
	 */
	inline uint32_t rotate2(const uint32_t node, const bool right_left) {
		assert(node != 0);
		auto & n = Elements<T>::_node[node].tree;
		assert(n.active);

		const uint32_t parent = n.parent;
		assert(parent != 0);
		auto & p = Elements<T>::_node[parent];
		assert(p.active);

		const uint32_t child = right_left ? n.left : n.right;
		assert(child != 0);
		auto & c = Elements<T>::_node[child];
		assert(c.active);

		if (right_left) {
			// Rotate right and then left
			if ((n.left = c.right) != 0)
				Elements<T>::_node[n.left].parent = node;
			c.right = node;

			if ((p.right = c.left) != 0)
				Elements<T>::_node[p.right].parent = parent;
			c.left = parent;
		} else {
			// Rotate left and then right
			if ((n.right = c.left) != 0)
				Elements<T>::_node[n.right].parent = node;
			c.left = node;

			if ((p.left = c.right) != 0)
				Elements<T>::_node[p.left].parent = parent;
			c.right = parent;
		}
		n.parent = p.parent = child;

		// Adjust balance
		n.balance = c.balance > 0 ? (right_left ? -1 :  1) : 0 ;
		p.balance = c.balance < 0 ? (right_left ?  1 : -1) : 0 ;
		c.balance = 0;

		return child;
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
