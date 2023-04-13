#include <stdio.h>

extern int bar;
extern int nested_foo();
extern int nested_bar();

int * bar_ptr = &bar;

int foo;

__attribute__((constructor)) static void extern_init() {
	int val = 42;
	printf("%s: setting foo @ %p to %d\n", __func__, &foo, val);
	foo = val;
}

int extern_bar() {
	printf("%s: bar @ %p\n", __func__, bar_ptr);
	return *bar_ptr;
}

int extern_foo() {
	printf("%s: foo @ %p\n", __func__, &foo);
	return foo;
}

int extern_nested_bar() {
	return nested_bar();
}

int extern_nested_foo() {
	return nested_foo();
}
