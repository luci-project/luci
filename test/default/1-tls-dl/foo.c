#include <stdbool.h>
#include "foo.h"

#define int128(HIGH,LOW) ((unsigned __int128)(HIGH) << 64 | (LOW))

#define FOO_SIZE 4097

__thread unsigned long foo_val = 0;
__thread bool foo_initialized = false;
__thread unsigned long foo_var[FOO_SIZE];

void foo_init(unsigned long val) {
	if (!foo_initialized) {
		foo_val = val;
		unsigned __int128 s = int128(foo_val,0xbadf00d1);
		const unsigned __int128 m = int128(0x12e15e35b500f16e,0x2e714eb2b37916a5);
		for (size_t i = 0; i < FOO_SIZE; i++)
			foo_var[i] = (s *= m) >> 64;
		foo_initialized = true;
	}
}

unsigned long foo_hash(size_t num) {
	if (!foo_initialized)
		return 0;
	unsigned long r = foo_val;
	for (size_t i = 0; i < FOO_SIZE && i < num; i++)
		r ^= foo_var[i];
	return r;
}
