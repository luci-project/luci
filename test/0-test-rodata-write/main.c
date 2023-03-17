#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

const int i = 23;
volatile int * iptr = (volatile int *)&i;

static void handler(int sig, siginfo_t *si, void *unused) {
	printf("Got signal %d -- exit!\n", sig);
	exit(EXIT_SUCCESS);
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

	puts("Try to write in rodata section");
	*iptr = 0xbadbeef;

	puts("Test failed: Writing on rodata section didn't stop app!");
	return EXIT_FAILURE;
}
