#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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
	sleep(2);
	for (int i = 0; i < 3; i++) {
		printf("Question %d: ", i);
		bool answered = ask();
		puts(answered ? "Ok!" : "What?");
		sleep(4);
	}

	return 0;
}
