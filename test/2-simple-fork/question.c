#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "answer.h"

const int buf_len = 10;

static bool ask() {
	puts("What is the answer to life the universe and everything?");

	char a[buf_len];
	if (answer(a, 10) > 0) {
		puts(a);
		return true;
	} else {
		puts("(no answer)");
		return false;
	}
}

int main() {
	int i = 0;
	pid_t f2 = -1, f1 = fork();
	if (f1 < 0)
		return 1;
	else if (f1 > 0) {
		i++;

		f2 = fork();
		if (f2 < 0)
			return 1;
		else if (f2 > 0)
			i++;
	}

	sleep(i * 4);

	printf("Question %d: ", i);
	bool answered = ask();
	puts(answered ? "Ok!" : "What?");

	if (f2 != 0)
		waitpid(f2, NULL, 0);
	if (f1 != 0)
		waitpid(f1, NULL, 0);

	return 0;
}
