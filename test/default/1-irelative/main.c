#include <stdio.h>

enum LANG {
	DE, EN, ES, FR, NO
};

__attribute__((weak)) enum LANG language = FR;

static const char * deutsch(void) {
	return "Hallo Welt";
}

static const char * english(void) {
	return "Hello World";
}

static const char * espanol(void) {
	return "Hola Mundo";
}

static const char * francais(void) {
	return "Bonjour le monde";
}

static const char * norsk(void) {
	return "Hei Verden";
}
static const char * (* resolve(void))(void) {
	switch (language) {
		case DE: return deutsch;
		case ES: return espanol;
		case FR: return francais;
		case NO: return norsk;
		default: return english;
	}
}

const char * localized(void) __attribute__((ifunc("resolve")));

int main() {
	printf("%s!\n", localized());
}
