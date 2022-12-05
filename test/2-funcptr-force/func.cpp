#include <iostream>
#include "func.hpp"

void foo(int i) {
	std::cout << "I am foo(" << i << ") @ v" << VERSION << std::endl;
}

static void bar(int i) {
	std::cout << "I am bar(" << i << ") @ v" << VERSION << std::endl;
}

func_t get_bar() {
	return &bar;
}

void baz(int i) {
	std::cout << "I am baz(" << i << ") @ v" << VERSION << std::endl;
}

static const func_t foo_ptr = foo;

void indirect_foo(int i) {
	foo_ptr(i);
}

static func_t bar_ptr = bar;

void oneshot_bar(int i) {
	if (bar_ptr == nullptr) {
		std::cout << "[function pointer is zeroed]" << std::endl;
	} else {
		bar_ptr(i);
		bar_ptr = nullptr;
	}
}

func_t baz_ptr = baz;
