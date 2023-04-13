#include <stdio.h>

extern int foo;
int * foo_ptr = &foo;
extern int * bar_ptr;
extern int extern_foo();
extern int extern_bar();
extern int extern_nested_foo();
extern int extern_nested_bar();


static void dump() {
	printf("foo @ %p = %d\n", foo_ptr, foo);
	printf("bar @ %p = %d\n", bar_ptr, *bar_ptr);
	int extern_foo_var = extern_foo();
	printf("extern_foo(): %d\n", extern_foo_var);
	int extern_bar_var = extern_bar();
	printf("extern_bar(): %d\n", extern_bar_var);
	int extern_nested_foo_var = extern_nested_foo();
	printf("extern_nested_foo(): %d\n", extern_nested_foo_var);
	int extern_nested_bar_var = extern_nested_bar();
	printf("extern_nested_bar(): %d\n", extern_nested_bar_var);
}

__attribute__((constructor)) static void main_init() {
	printf("[%s]\n", __func__);
	dump();
	printf("foo *= -1\n");
	foo *= -1;
}


int main(int argc, char ** argv) {
	printf("[%s]\n", __func__);
	dump();
	return 0;
}
