#include <iostream>

#include "bar.hpp"
#include "baz.hpp"

int bar(int i) {
	std::cout << "  bar: i = " << i << std::endl;
	return baz(i);
}
