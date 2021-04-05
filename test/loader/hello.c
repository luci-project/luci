#include <stddef.h>
#include <syscall.h>
#include "sys.h"

static char boing[] = "Boing boing boing!\n";
char text[] = "Hello, world!\n";

int main() {
	long result;
	asm volatile (
		"syscall"
		: "=a"(result)
		: "0"(__NR_write), "D"(2), "S"(boing), "d"(sizeof(boing) - 1)
		: "cc", "rcx", "r11", "memory"
	);

	sys_write(1, text, sizeof(text) - 1);
	return 0;
}
