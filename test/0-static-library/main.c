#include <stdio.h>
#include <stdlib.h>

#include "lib.h"

int main() {
	if (lorem_ipsum() != 23)
		return 1;

	printf("%s, %s\n", ut_enim, quis_nostrud);

	quis_aute(42);

	char * e = excepteur();
	if (e == NULL)
		return 1;
	puts(e);
	free(e);

	return 0;
}
