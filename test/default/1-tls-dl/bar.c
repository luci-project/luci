#include <stdbool.h>
#include "bar.h"

#define BAR_SIZE 1025
static __thread unsigned long bar_val = 0;
static __thread bool bar_initialized = false;
static __thread unsigned long bar_var[BAR_SIZE];

void bar_init(unsigned long val) {
	if (!bar_initialized) {
		bar_val = val;
		for (size_t i = 0; i < BAR_SIZE; i++)
			bar_var[i] = val = (val >> 1) ^ (val | val << 1);
		bar_initialized = true;
	}
}

unsigned long bar_hash(size_t num) {
	if (!bar_initialized)
		return 0;
	unsigned long r = bar_val;
	for (size_t i = 0; i < BAR_SIZE && i < num; i++)
		r ^= bar_var[i];
	return r;
}
