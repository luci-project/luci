#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
 #include <dlfcn.h>


int (*answer)() = NULL;

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
	char * error;

	void *handle = dlopen("libanswer.so", RTLD_NOW);
	if (!handle) {
		fprintf(stderr, "dlopen: %s\n", dlerror());
		return EXIT_FAILURE;
	}

	dlerror();
	answer = dlsym(handle, "answer");
	if ((error = dlerror()) != NULL) {
		fprintf(stderr, "dlsym (answer): %s\n", error);
		return EXIT_FAILURE;
	}

	dlerror();
	const char * has_answer_init = dlsym(handle, "has_answer");
	if ((error = dlerror()) != NULL) {
		fprintf(stderr, "dlsym (has_answer_init): %s\n", error);
		return EXIT_FAILURE;
	}

	for (int i = 0; i < 3; i++) {
		if (i != 0)
			sleep(4);

		printf("Question %d: ", i);
		bool answered = ask();
		puts(answered ? "Ok!" : "What?");

		dlerror();
		const char * has_answer = dlsym(handle, "has_answer");
		if ((error = dlerror()) != NULL) {
			fprintf(stderr, "dlsym (has_answer): %s\n", error);
		} else {
			printf("(Please note: %s)\n", has_answer);
		}
	}

	printf("At the end, we conclude: %s\n", has_answer_init);
	return 0;
}
