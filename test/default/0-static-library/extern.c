#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nested.h"

const char * const ut_enim = "Ut enim ad minim veniam";
const char * quis_nostrud;

static char * consectetur = NULL;
static const size_t buflen = 250;

static void imp_quis_aute(int);

void (*quis_aute)(int) = &imp_quis_aute;

__attribute__((constructor)) void init() {
	quis_nostrud = "quis nostrud exercitation ullamco laboris nisi ut aliquid ex ea commodi consequat.";
	consectetur = strdup("consectetur adipisici elit");
}

__attribute__((destructor)) void deinit() {
	free(consectetur);
}

int lorem_ipsum() {
	printf("Lorem ipsum dolor sit amet, %s, %s.\n", consectetur, "sed eiusmod tempor incidunt ut labore et dolore magna aliqua");
	return 23;
}

static void imp_quis_aute(int key) {
	char code[] = "'K?IT7KJ;T?KH;TH;FH;>;D:;H?JT?DTLEBKFJ7J;TL;B?JT;II;T9?BBKCT:EBEH;T;KT<K=?7JTDKBB7TF7H?7JKHb";
	const char min = ' ';
	const char max = '~';
	while (key < 0)
		key += max - min;
	for (size_t n = 0; n < sizeof(code); n++)
		if (code[n] >= min && code[n] <= max)
			code[n] = ((code[n] + key - min) % (max - min)) + min;
	puts(code);
}

char * excepteur() {
	char * buf = malloc(buflen);
	if (buf != NULL) {
		size_t elen = nested_excepteur(buf, buflen);
		strncat(buf + elen, ", ", buflen - elen);
		strncat(buf + elen + 2, nested_sunt_in, buflen - elen - 2);
	}
	return buf;
}
