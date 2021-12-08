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
	for (int i = 0; i < 3; i++) {
		if (i != 0)
			sleep(4);

		printf("Question %d: ", i);
		bool answered = ask();
		puts(answered ? "Ok!" : "What?");
	}

	return 0;
}
