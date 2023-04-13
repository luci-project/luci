#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

// Machine code bytes for `call *%rdi; nop ret;`
char exec_data[]  = {'\xff', '\xd7', '\xc3', '\x90', '\0' };

static void handler(int sig, siginfo_t *si, void *unused) {
	printf("Got signal %d -- exit!\n", sig);
	exit(EXIT_SUCCESS);
}

void fail() {
	puts("Test failed: executing code from .data!");
	exit(EXIT_FAILURE);
}

int main() {
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = handler;
	if (sigaction(SIGSEGV, &sa, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}

	// Try to exec code in data section
	puts("Try to exec code in data section");
	void(*fp)(void(*)()) = (void(*)(void(*)()))(exec_data);
	fp(fail);

	puts("Test failed: Executing code from .data didn't stop app!");
	return EXIT_FAILURE;
}
