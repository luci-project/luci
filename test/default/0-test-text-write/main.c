#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

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

	// Try to write on code segment
	puts("Try to write on code segment");
	volatile void ** ptr = (volatile void**)(main);
	*ptr = (void*)(0xbadbeef);

	puts("Test failed: Writing on code segment didn't stop app!");
	return EXIT_FAILURE;
}
