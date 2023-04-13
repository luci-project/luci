#include "answer.h"

const int base = 10;

int answer(char * buffer, int len) {
#ifdef ANSWER
	int required_len = 0;
	int tmp = ANSWER;
	do {
		tmp /= base;
		required_len++;
	} while (tmp != 0);
	if (required_len <= len) {
		int p = required_len;
		buffer[p] = '\0';
		int tmp = ANSWER;
		while (p-- != 0) {
			buffer[p] = tmp % base + '0';
			tmp /= base;
		}
		return required_len;
	}
#endif
	return -1;
}
