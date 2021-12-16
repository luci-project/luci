#include <stddef.h>
#include <assert.h>

// from http://www.math.sci.hiroshima-u.ac.jp/m-mat/MT/VERSIONS/C-LANG/mt19937-64.c
#define VEC_LEN 312
__thread short minit = 0;
__thread unsigned long mt[VEC_LEN] = { 19650218UL };
__thread size_t mti = 1;
const unsigned long mag01[2]={0UL, 0xB5026F5AA96619E9UL};

void baz_helper_init(unsigned long val) {
	if (minit == 0) {
		for (; mti < VEC_LEN; mti++)
			mt[mti] = (6364136223846793005UL * (mt[mti - 1] ^ (mt[mti - 1] >> 62)) + mti);
		size_t i = 1;
		for (size_t k = VEC_LEN; k; k--) {
			mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 62)) * 3935559000370003845UL)) + val;
			if (++i >= VEC_LEN) {
				mt[0] = mt[VEC_LEN - 1];
				i = 1;
			}
		}
		for (size_t k = VEC_LEN - 1; k; k--) {
			mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 62)) * 2862933555777941757UL)) - i;
			if (++i >= VEC_LEN) {
				mt[0] = mt[VEC_LEN - 1];
				i = 1;
			}
		}
		mt[0] = 1UL << 63;
		minit = 1;
	}
}

unsigned long baz_helper() {
	assert(minit != 0);
	if (mti >= VEC_LEN) {
		size_t i;
		for (i = 0; i < VEC_LEN / 2; i++) {
			unsigned long v = (mt[i] & 0xFFFFFFFF80000000UL) | (mt[i + 1] & 0x7FFFFFFFUL);
			mt[i] = mt[i + VEC_LEN / 2] ^ (v >> 1) ^ mag01[(int)(v & 1UL)];
		}
		for (; i < VEC_LEN - 1; i++) {
 			unsigned long v = (mt[i] & 0xFFFFFFFF80000000UL) | (mt[i + 1] & 0x7FFFFFFFUL);
			mt[i] = mt[i + VEC_LEN / 2 - VEC_LEN] ^ (v >> 1) ^ mag01[(int)(v & 1UL)];
		}
		unsigned long v = (mt[VEC_LEN - 1] & 0xFFFFFFFF80000000UL) | (mt[0] & 0x7FFFFFFFUL);
		mt[VEC_LEN - 1] = mt[VEC_LEN / 2 - 1] ^ (v >> 1) ^ mag01[(int)(v & 1UL)];

		mti = 0;
	}

	unsigned long r = mt[mti++];

	r ^= (r >> 29) & 0x5555555555555555UL;
	r ^= (r << 17) & 0x71D67FFFEDA60000UL;
	r ^= (r << 37) & 0xFFF7EEE000000000UL;
	r ^= (r >> 43);

	return r;
}
