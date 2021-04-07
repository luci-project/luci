#include <stddef.h>
#include <syscall.h>
#include "sys.h"

static char boing[] = "Boing boing boing!\n";
char text_en[] = "Hello world!\n";
char text_de[] = "Hallo Welt!\n";
char text_es[] = "Hola Mundo!\n";
char text_fr[] = "Bonjour Monde!\n";
char text_no[] = "Hei Verden!\n";



int main() {
	long result;
	asm volatile (
		"syscall"
		: "=a"(result)
		: "0"(__NR_write), "D"(1), "S"(boing), "d"(sizeof(boing) - 1)
		: "cc", "rcx", "r11", "memory"
	);

	sys_write(1, text_en, sizeof(text_en) - 1);
	sys_write(1, text_de, sizeof(text_de) - 1);
	sys_write(1, text_es, sizeof(text_es) - 1);
	sys_write(1, text_fr, sizeof(text_fr) - 1);
	sys_write(1, text_no, sizeof(text_no) - 1);
	return 0;
}
