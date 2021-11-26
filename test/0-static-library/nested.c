#include <string.h>

const char * const nested_sunt_in = "sunt in culpa qui officia deserunt mollit anim id est laborum.";

size_t nested_excepteur(char * dest, size_t n) {
	if (dest != NULL) {
		strncpy(dest, "Excepteur sint obcaecat cupiditat non proident", n);
		dest[n - 1] = '\0';
		return strlen(dest);
	}
	return 0;
}
