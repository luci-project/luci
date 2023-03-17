#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static void handler(int sig, siginfo_t *si, void *unused) {
	printf("Got signal %d -- exit!\n", sig);
	exit(EXIT_SUCCESS);
}

void fail() {
	puts("Test failed: executing code from stack!");
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

	// Try to exec code in stack
	char exec[3];
	// Machine code bytes for `call *%rdi`
	exec[0] = '\xff';
	exec[1] = '\xd7';
	// Machine code bytes for `ret`
	exec[2] = '\xc3';

	puts("Try to exec code on stack");
	void(*fp)(void(*)()) = (void(*)(void(*)()))(exec);
	fp(fail);

	puts("Test failed: Executing code from stack didn't stop app!");
	return EXIT_FAILURE;
}
