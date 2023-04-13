#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static void handler(int sig, siginfo_t *si, void *unused) {
	printf("Got signal %d -- exit!\n", sig);
	exit(EXIT_SUCCESS);
}

void fail() {
	puts("Test failed: executing code from relocation-read-only sectio!");
	exit(EXIT_FAILURE);
}

struct Temp {
	void * foo;
	char i[4];
	void * bar;
};
const struct Temp tmp = { (void*)&handler, {'\xff', '\xd7', '\xc3', '\x90'}, (void*)&handler };

int main() {
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = handler;
	if (sigaction(SIGSEGV, &sa, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}

	puts("Try to execute code in relocation-read-only section");
	void(*fp)(void(*)()) = (void(*)(void(*)()))(tmp.i);
	fp(fail);

	puts("Test failed: Executing code in relocation-read-only section didn't stop app!");
	return EXIT_FAILURE;
}
