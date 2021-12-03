#include <stdio.h>

#include "greek.h"

static char * lib = "three";

const char * libname() {
	return lib;
}

int _gamma(char * str, size_t size)  {
	return snprintf(str, size, "%s:%s!", lib, __func__);
}

int _delta(char * str, size_t size)  {
	size_t printed = snprintf(str, size, "%s:%s -> ", lib, __func__);
	return printed + (printed < size && _alpha ? _alpha(str + printed, size - printed) : 0);
}

int _epsilon(char * str, size_t size)  {
	size_t printed = snprintf(str, size, "%s:%s -> ", lib, __func__);
	return printed + (printed < size && _beta ? _beta(str + printed, size - printed) : 0);
}

int _zeta(char * str, size_t size) {
	size_t printed = snprintf(str, size, "%s:%s -> ", lib, __func__);
	return printed + (printed < size && _gamma ? _gamma(str + printed, size - printed) : 0);
}

int _eta(char * str, size_t size) {
	size_t printed = snprintf(str, size, "%s:%s -> ", lib, __func__);
	return printed + (printed < size && _delta ? _delta(str + printed, size - printed) : 0);
}

int _theta(char * str, size_t size)  {
	size_t printed = snprintf(str, size, "%s:%s -> ", lib, __func__);
	return printed + (printed < size && _epsilon ? _epsilon(str + printed, size - printed) : 0);
}
