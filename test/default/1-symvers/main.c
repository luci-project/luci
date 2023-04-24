#include <stdio.h>

asm (".symver foo_v0, foo@VERS_0");
asm (".symver foo_v2, foo@VERS_2");

extern int foo_v0();
extern int foo_v2();
extern int foo();

int main() {
	printf("vers. 0: %d\n", foo_v0());
	printf("vers. 2: %d\n", foo_v2());
	printf("default: %d\n", foo());
	return 0;
}