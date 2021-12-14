#include <stdio.h>
#include <unistd.h>

#include "foo.h"

const char * msgs[] = { "one", "two", "three", NULL };

int main(int argc, char **argv) {
	puts("Foo example");
	puts("===========");

	for (size_t i = 0; i < 4 * 9; i++) {
		printf("\n\e[1m[Run %zu, foo_inc() = %d]\e[0m\n", i, foo_inc());
		foo_show(msgs[i % 4]);
		sleep(2);
	}

	puts("\n===========");

	return 0;
}
