#include "answer.h"

#include <stdlib.h>

#ifdef ALTERNATIVE
#define ANSWER 23
#else
#define ANSWER 42
#endif

const int base = 10;

int answer(char * buffer, int len) {
	int required_len = 0;
	int tmp = ANSWER;
	do {
		tmp /= base;
		required_len++;
	} while (tmp != 0);
	if (required_len > len) {
		return -1;
	} else {
		int p = required_len;
		buffer[p] = '\0';
		int tmp = ANSWER;
		while (p-- != 0) {
			buffer[p] = tmp % base + '0';
			tmp /= base;
		}
		return required_len;
	}
}
