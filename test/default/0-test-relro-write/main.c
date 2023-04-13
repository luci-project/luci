#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static void handler(int sig, siginfo_t *si, void *unused) {
	printf("Got signal %d -- exit!\n", sig);
	exit(EXIT_SUCCESS);
}

struct Temp {
	void * foo;
	int i;
	void * bar;
};
const struct Temp tmp = { (void*)&handler, 23, (void*)&handler };
volatile int * iptr = (volatile int *)&tmp.i;

int main() {
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = handler;
	if (sigaction(SIGSEGV, &sa, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}

	puts("Try to write in relocation-read-only section");
	*iptr = 0xbadbeef;

	puts("Test failed: Writing on relocation-read-only segment didn't stop app!");
	return EXIT_FAILURE;
}
