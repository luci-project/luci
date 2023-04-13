#include "answer.h"

#define XSTR(x) #x
#define STR(x) XSTR(x)

const int base = 10;

const char has_answer[] =
#ifdef ANSWER
	"This library provides an answer (" STR(ANSWER) ")!";
#else
	"This library does not provide any useful answers!";
#endif

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
