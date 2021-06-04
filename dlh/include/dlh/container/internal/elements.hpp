#pragma once

#include <cassert>
#include <cstdlib>

#include "../../utility.hpp"

template<class T>
class Elements {
 protected:
	uint32_t _capacity;
	uint32_t _next;
	uint32_t _count;

	struct Node {
		union {
			struct {
				bool active;
				uint32_t prev, next, temp;
			} hash;
			struct {
				bool active;
				int8_t balance;
				uint32_t parent, left, right;
			} tree;
		};

		T data;

		T& value() { return data; }
	} * _node;

	static_assert(sizeof(Node) == 16 + sizeof(T), "Wrong Node size");

	constexpr Elements() : _capacity(0), _next(1), _count(0), _node(nullptr) {}

	Elements(const Elements<T>& e) : _capacity(e._capacity), _next(e._next), _count(e._count), _node(malloc(e._capacity * sizeof(Node))) {
		assert(_node != nullptr);
		memcpy(_node, e._node, e._capacity * sizeof(Node));
	}

	Elements(Elements<T>&& e) : _capacity(move(e._capacity)), _next(move(e._next)), _count(move(e._count)), _node(move(e._node)) {
		assert(_node != nullptr);
		e._capacity = 0;
		e._next = 1;
		e._count = 0;
		e._node = 0;
	}

	virtual ~Elements() {
		free(Elements<T>::_node);
	}

	bool resize(uint32_t capacity) {
		if (capacity != _capacity) {
			void * ptr = realloc(_node, capacity * sizeof(Node));
			if (ptr == nullptr) {
				return false;
			} else {
				_node = reinterpret_cast<Node *>(ptr);
				_capacity = capacity;
			}
		}
		return true;
	}
};
