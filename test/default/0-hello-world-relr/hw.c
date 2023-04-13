#include <stdio.h>

const char *str[] = {
	"Hallo ", "welt", "!\n",
	"Hello ", "world", "!\n",
	"Hola ",  "mundo", "!\n",
	"Bonjour ", "monde", "!\n",
	NULL
};

int main() {
	for (const char ** ptr = str; *ptr != NULL; ptr++)
		printf("%s", *ptr);

	return 0;
}
