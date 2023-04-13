#include <stdio.h>

extern int foo;
int * foo_ptr = &foo;

int bar = 23;

int nested_bar() {
	printf("%s: bar @ %p\n", __func__, &bar);
	return bar;
}

int nested_foo() {
	printf("%s: foo @ %p\n", __func__, foo_ptr);
	return *foo_ptr;
}
