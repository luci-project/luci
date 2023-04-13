#include <unistd.h>
#include "func.hpp"

auto main_foo = &foo;

int main() {
	auto foo_ptr = &foo;
	auto bar_ptr = get_bar();
	for (int i = 0; i < 3; i++) {
		if (i > 0)
			sleep(5);
		foo_ptr(i);
		bar_ptr(i);
		baz(i);
		main_foo(i);
		oneshot_bar(i);
		baz_ptr(i);
		indirect_foo(i);
	}
}
