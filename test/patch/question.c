#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

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
	bool result = ask();
	
	return result ? EXIT_SUCCESS : EXIT_FAILURE;
}
