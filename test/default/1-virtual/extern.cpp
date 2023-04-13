#include <iostream>

#include "extern.h"

struct Base {
	int foo;

	Base() : foo(0) {}

	virtual const char * bar() {
		return  "base";
	}

	const char * baz() {
		return  "base";
	}
};

struct A : virtual Base {
	int a;

	A() : a(23) {
		foo = a;
	}

	virtual const char * bar() {
		return "A";
	}
};

struct B : virtual Base {
	int b;

	B() : b(42) {
		foo = b;
	}

	virtual const char * bar() {
		return "B";
	}

	const char * baz() {
		return  "B";
	}
};

struct Derived : A, B {
	Derived() {
		foo = a + b;
	}

	virtual const char * bar() {
		return "derived";
	}

	const char * baz() {
		return  "derived";
	}
};

static Base base;
static A a;
static B b;
static Derived derived;

template<typename T> void dump_cast(T &t) {
	std::cout << t.bar() << " " << t.baz() << " " << t.foo << std::endl;

	T & upcast = derived;
	std::cout << upcast.bar() << " " << upcast.baz() << " " << upcast.foo << std::endl;

	T * upcast_ptr = &derived;
	std::cout << upcast_ptr->bar() << " " << upcast_ptr->baz() << " " << upcast_ptr->foo << std::endl;

	Derived & downcast = dynamic_cast<Derived&>(upcast);
	std::cout << downcast.bar() << " " << downcast.baz() << " " << downcast.foo << std::endl;

	Base * base_ptr = &t;
	std::cout << base_ptr->bar() << " " << base_ptr->baz() << " " << base_ptr->foo << std::endl;

	std::cout << std::endl;
}

void dump(int n) {
	std::cout << n << ':' << std::endl;
	dump_cast(base);
	dump_cast(a);
	dump_cast(b);
	dump_cast(derived);
}
