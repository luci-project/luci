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

void boring(int i) {
	std::cout << "I am boring(" << i << ")" << std::endl;
}
