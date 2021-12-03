#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <gnu/lib-names.h>

int main() {
	void *handle = dlopen(LIBM_SO, RTLD_NOW);
	if (!handle) {
		fprintf(stderr, "dlopen: %s\n", dlerror());
		return EXIT_FAILURE;
	}

	dlerror();
	double (*cosine)(double) = (double (*)(double)) dlsym(handle, "cos");

	char * error = dlerror();
	if (error != NULL) {
		fprintf(stderr, "dlsym: %s\n", error);
		return EXIT_FAILURE;
	}

	double value = 2.0;
	printf("cos(%.4f) = %.4f\n", value, (*cosine)(value));

	dlclose(handle);
	return EXIT_SUCCESS;
}
