#include "foo.h"
#include <stdio.h>
#include <string.h>

int var = 23;

#if VERSION == 4
int newvar = 1337;  // should fail
#endif

#if VERSION == 5
const int newconst = 42;
#endif

int foo_inc() {
#if VERSION >= 1
	var++;
#endif
	return var;
}

#if VERSION >= 7
extern int foo_dec();
int foo_dec() {
	return var--;
}
#endif

int foo_show(const char * value) {
	static unsigned long long call = 0;
	size_t len = value == NULL ? 0 : strlen(value);
#if VERSION <= 1
	printf("Foo (Version %d):\n\tvar = %d\n\tvalue = %s (%zu)\n\tcall = %llu\n", VERSION, var, value, len, ++call);
#elif VERSION == 2
	printf("Foo (Version 2):\n - var = %d\n - value = %s (%zu)\n - call = %llu\n", var, value, len, ++call);
#elif VERSION >= 3
	int version = VERSION;
	printf("Foo (Version %d):\n", version);
	printf(" - var = %d\n", var);
#if VERSION == 4
	printf(" - newvar = %d\n", newvar);
#endif
#if VERSION == 5
	printf(" - newconst = %d\n", newconst);
#endif
#if VERSION >= 6
	if (value == NULL)
		value = "[UNDEFINED]";
#endif
	printf(" - value = %s (%zu)\n", value, len);
	printf(" - call = %llu\n", ++call);
#endif
#if VERSION >= 8
	puts("Incompatible new function!");  // should fail
#endif
	return VERSION;
}
