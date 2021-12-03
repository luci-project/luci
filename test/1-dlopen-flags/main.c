#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include "greek.h"

int _theta(char * str, size_t size) {
	size_t printed = snprintf(str, size, "main:%s -> ", __func__);
	return printed + (printed < size && _gamma ? _gamma(str + printed, size - printed) : 0);
}

int _lambda(char * str, size_t size) {
	size_t printed = snprintf(str, size, "main:%s -> ", __func__);
	return printed + (printed < size  && _theta ? _theta(str + printed, size - printed) : 0);
}

const size_t bufsz = 100;

static int check_helper(greek_t func, const char * name) {
	int r = -1;
	if (func) {
		char buf[bufsz + 1];
		buf[bufsz] = '\0';
		r = func(buf, bufsz);
		printf("%s: %s\n", name, buf);
	} else {
		printf("(no %s)\n", name);
	}
	return r;
}

#define check(FUNC) check_helper(FUNC, #FUNC)

static const char * syms[] = {
	"_alpha",
	"_beta",
	"_gamma",
	"_delta",
	"_epsilon",
	"_zeta",
	"_eta",
	"_theta",
	"_iota",
	"_kappa",
	"_lambda"
};

static void check_default() {
	puts("\nDefault symbols:");
	for (size_t s = 0; s < sizeof(syms)/sizeof(syms[0]); s++) {
		greek_t func = dlsym(RTLD_DEFAULT, syms[s]);
		check_helper(func, syms[s]);
	}
}

static greek_t libsym(void * handle, const char * sym) {
	greek_t func = dlsym(handle, sym);
	char * error = dlerror();
	if (!func) {
		fprintf(stderr, "dlsym of %s failed: %s\n", sym, error);
		exit(EXIT_FAILURE);
	}
	return func;
}

static int libcheck(const char *filename, int flags) {
	if (dlopen(filename, flags | RTLD_NOLOAD) == NULL) {
		printf("%s is not loaded (yet).\n", filename);
		return 0;
	} else {
		printf("%s is already loaded.\n", filename);
		return 1;
	}
}

static void * libopen(const char *filename, int flags) {
	void *handle = dlopen(filename, flags);
	const char * error = dlerror();
	if (!handle) {
		fprintf(stderr, "dlopen of %s failed: %s\n", filename, error);
		exit(EXIT_FAILURE);
	} else {
		printf("\nDynamically loaded %s\n", filename);
	}
	return handle;
}


int main() {
	printf("\nLoaded with library '%s'\n", libname());
	check(_alpha);
	check(_beta);
	check(_gamma);
	check(_delta);
	check(_epsilon);
	check(_zeta);
	check(_eta);
	check(_theta);
	check(_iota);
	check(_kappa);
	check(_lambda);

	check_default();

	void *two = libopen("libtwo.so", RTLD_LAZY | RTLD_LOCAL);
	dlerror();
	greek_t two_delta = libsym(two, "_delta");
	check(two_delta);
	greek_t two_eta = libsym(two, "_eta");
	check(two_eta);
	check_default();

	void *three = libopen("libthree.so", RTLD_LAZY | RTLD_LOCAL);
	dlerror();
	greek_t three_delta = libsym(three, "_delta");
	check(three_delta);
	greek_t three_eta = libsym(three, "_eta");
	check(three_eta);
	check_default();

	libcheck("libthree.so", RTLD_LAZY | RTLD_GLOBAL);
	check_default();

	void *four = libopen("libfour.so", RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
	dlerror();
	greek_t four_delta = libsym(four, "_delta");
	check(four_delta);
	greek_t four_eta = libsym(four, "_eta");
	check(four_eta);
	greek_t four_iota = libsym(four, "_iota");
	check(four_iota);
	check_default();

	puts("\nChecking libs:");
	libcheck("libone.so", RTLD_LAZY | RTLD_GLOBAL);
	libcheck("libtwo.so", RTLD_LAZY | RTLD_LOCAL);
	libcheck("libthree.so", RTLD_LAZY | RTLD_GLOBAL);
	libcheck("libfour.so", RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
	libcheck("libfive.so", RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND);

	void *five = libopen("libfive.so", RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND);
	dlerror();
	greek_t five_eta = libsym(five, "_eta");
	check(five_eta);
	greek_t five_iota = libsym(five, "_iota");
	check(five_iota);
	check_default();

	return 0;
}
