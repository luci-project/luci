#include <stdbool.h>
#include <stddef.h>
#include "baz-helper.h"

#define BAZ_SIZE 16999
__thread unsigned long baz_val = 0;
__thread bool baz_initialized = false;
__thread unsigned long baz_var[BAZ_SIZE];

void baz_init(unsigned long val) {
	if (!baz_initialized) {
		baz_helper_init(baz_val = val);

		for (size_t j = 0; j < BAZ_SIZE; j++)
			baz_var[j] = baz_helper();

		baz_initialized = true;
	}
}

unsigned long baz_hash(size_t num) {
	if (!baz_initialized)
		return 0;
	unsigned long r = baz_val;
	for (size_t i = 0; i < BAZ_SIZE && i < num; i++)
		r ^= baz_var[i];
	return r;
}
