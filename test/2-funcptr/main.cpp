#include <unistd.h>
#include "func.hpp"

int main() {
	auto foo_ptr = &foo;
	auto bar_ptr = get_bar();
	for (int i = 0; i < 3; i++) {
		if (i > 0)
			sleep(3);
		foo_ptr(i);
		bar_ptr(i);
		baz(i);
	}
}
