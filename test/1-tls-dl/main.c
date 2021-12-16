#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

#include "foo.h"
#include "bar.h"

static void * foobar(void * param) {
	unsigned long val = (unsigned long)param;
	size_t r = 128;
	printf("foo (no init) range %05zu: %016lx (foobar)\n", r, foo_hash(r));
	printf("bar (no init) range %05zu: %016lx (foobar)\n", r, bar_hash(r));

	foo_init(val);
	bar_init(val);
	sleep(1);
	for (size_t i = 1; i < (1 << 15); i *= 2) {
		usleep(50000);
		printf("foo init %04lu range %05zu: %016lx (foobar)\n", val, i, foo_hash(i));
		usleep(50000);
		printf("bar init %04lu range %05zu: %016lx (foobar)\n", val, i, bar_hash(i));
	}
	return param;
}

static void * foobarbaz(void * param) {
	unsigned long val = (unsigned long)param;

	void *baz_handle = dlopen("libbaz.so", RTLD_LAZY);
	if (!baz_handle) {
		fprintf(stderr, "dlopen: %s\n", dlerror());
		return NULL;
	}

	dlerror();
	void (*baz_init)(unsigned long) = (void (*)(unsigned long)) dlsym(baz_handle, "baz_init");
	char * error = dlerror();
	if (error != NULL) {
		fprintf(stderr, "dlsym baz_init: %s\n", error);
		return NULL;
	}

	dlerror();
	unsigned long (*baz_hash)(size_t) = (unsigned long (*)(size_t)) dlsym(baz_handle, "baz_hash");
	error = dlerror();
	if (error != NULL) {
		fprintf(stderr, "dlsym baz_hash: %s\n", error);
		return NULL;
	}

	size_t r = 128;
	printf("foo (no init) range %05zu: %016lx (foobarbaz)\n", r, foo_hash(r));
	printf("baz (no init) range %05zu: %016lx (foobarbaz)\n", r, baz_hash(r));
	printf("bar (no init) range %05zu: %016lx (foobarbaz)\n", r, bar_hash(r));

	foo_init(val);
	baz_init(val);
	bar_init(val);
	sleep(1);
	for (size_t i = 1; i < (1 << 15); i *= 2) {
		usleep(33333);
		printf("foo init %04lu range %05zu: %016lx (foobarbaz)\n", val, i, foo_hash(i));
		usleep(33333);
		printf("baz init %04lu range %05zu: %016lx (foobarbaz)\n", val, i, baz_hash(i));
		usleep(33333);
		printf("bar init %04lu range %05zu: %016lx (foobarbaz)\n", val, i, bar_hash(i));
	}

	dlclose(baz_handle);

	return param;
}


int main() {
	pthread_t foobar23, foobar42, foobar1337, foobarbaz23, foobarbaz42, foobarbaz1337;

	unsigned long foo_init_val = 23;
	foo_init(foo_init_val);
	size_t foo_range = 128;
	printf("foo init %04lu range %05zu: %016lx (main)\n", foo_init_val, foo_range, foo_hash(foo_range));

	if (pthread_create(&foobar23, NULL, foobar, (void*)23)) {
		fprintf(stderr, "Error creating foobar23 thread\n");
		return 1;
	} else if (pthread_create(&foobar42, NULL, foobar, (void*)42)) {
		fprintf(stderr, "Error creating foobar23 thread\n");
		return 1;
	} else if (pthread_create(&foobarbaz23, NULL, foobarbaz, (void*)23)) {
		fprintf(stderr, "Error creating foobarbaz23 thread\n");
		return 1;
	} else if (pthread_create(&foobarbaz42, NULL, foobarbaz, (void*)42)) {
		fprintf(stderr, "Error creating foobarbaz23 thread\n");
		return 1;
	} else if (pthread_create(&foobar1337, NULL, foobar, (void*)1337)) {
		fprintf(stderr, "Error creating foobar1337 thread\n");
		return 1;
	} else if (pthread_create(&foobarbaz1337, NULL, foobarbaz, (void*)1337)) {
		fprintf(stderr, "Error creating foobarbaz1337 thread\n");
		return 1;
	}

	foo_range = 256;
	printf("foo init %04lu range %05zu: %016lx (main)\n", foo_init_val, foo_range, foo_hash(foo_range));
	sleep(1);
	foo_range = 512;
	printf("foo init %04lu range %05zu: %016lx (main)\n", foo_init_val, foo_range, foo_hash(foo_range));
	sleep(1);
	foo_range = 1024;
	printf("foo init %04lu range %05zu: %016lx (main)\n", foo_init_val, foo_range, foo_hash(foo_range));

	void *ret = NULL;
	if (pthread_join(foobar23, &ret)) {
		fprintf(stderr, "Error joining foobar23 thread\n");
		return 2;
	} else if (ret != (void*)23) {
		fprintf(stderr, "Invalid return value of foobar23: %p\n", ret);
		return 3;
	}
	if (pthread_join(foobar42, &ret)) {
		fprintf(stderr, "Error joining foobar42 thread\n");
		return 2;
	} else if (ret != (void*)42) {
		fprintf(stderr, "Invalid return value of foobar42: %p\n", ret);
		return 3;
	}
	if (pthread_join(foobarbaz23, &ret)) {
		fprintf(stderr, "Error joining foobarbaz23 thread\n");
		return 2;
	} else if (ret != (void*)23) {
		fprintf(stderr, "Invalid return value of foobarbaz23: %p\n", ret);
		return 3;
	}
	if (pthread_join(foobarbaz42, &ret)) {
		fprintf(stderr, "Error joining foobarbaz42 thread\n");
		return 2;
	} else if (ret != (void*)42) {
		fprintf(stderr, "Invalid return value of foobarbaz42: %p\n", ret);
		return 3;
	}
	if (pthread_join(foobar1337, &ret)) {
		fprintf(stderr, "Error joining foobar1337 thread\n");
		return 2;
	} else if (ret != (void*)1337) {
		fprintf(stderr, "Invalid return value of foobar1337: %p\n", ret);
		return 3;
	}
	if (pthread_join(foobarbaz1337, &ret)) {
		fprintf(stderr, "Error joining foobarbaz1337 thread\n");
		return 2;
	} else if (ret != (void*)1337) {
		fprintf(stderr, "Invalid return value of foobarbaz1337: %p\n", ret);
		return 3;
	}
	return 0;
}
