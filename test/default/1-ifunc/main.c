#include <stdio.h>

#include "extern.h"

enum LANG language = NO;

int main() {
	printf("%s!\n", localized());
}
