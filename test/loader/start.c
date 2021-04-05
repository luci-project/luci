#include <stddef.h>
#include "sys.h"

extern int main();

void _start() {
	int r = main();
	sys_exit(r);
}
