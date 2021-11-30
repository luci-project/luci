#include <signal.h>
#include <stdio.h>
#include <unistd.h>

void sig_handler(int signum) {
	puts("signal handler");
}

int main() {
	signal(SIGTRAP, sig_handler);
	puts("start");
	asm("int3");
	puts("end");
	return 0;
}
