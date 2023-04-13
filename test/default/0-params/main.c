#include <stdlib.h>
#include <stdio.h>
#include <sys/auxv.h>


int main(int argc, const char * argv[]) {
	printf("This program was called with %d. arguments:\n", argc);
	for (int a = 1; a < argc; a++) {
		printf(" - the %d. argument is '%s'\n", a, argv[a]);
	}

	const char * env_name = "PARAM_TEST_NAME";
	const char * env_var = getenv(env_name);
	if (env_var == NULL) {
		fprintf(stderr, "There is no environment variable named '%s'\n", env_name);
	} else {
		printf("The environment variable '%s' is set to '%s'\n", env_name, env_var);
	}

	unsigned long phentry_size = getauxval(AT_PHENT);
	if (phentry_size == 0) {
		fprintf(stderr, "There is no auxilary vector of type 'AT_PHENT'\n");
	} else {
		printf("The auxilary vector of type 'AT_PHENT' is set to %lu\n", phentry_size);
	}

	return 0;
}
