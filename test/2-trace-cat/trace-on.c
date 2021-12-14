#include "sys.h"

static void logbuf(const char * msg, size_t len) {
	for (size_t p = 0; p < len; ) {
		ssize_t w = sys_write(2, msg + p, len - p);
		if (w < 0)
			sys_exit(23);
		else
			p += w;
	}
}

static void logmsg(const char * msg) {
	size_t len;
	if (msg == NULL)
		msg = "NULL";
	for (len = 0; msg[len] != '\0'; ++len) {}

	logbuf(msg, len);
}

static void lognum(long val, int base) {
	const unsigned long len = 67;
	char buf[len];
	unsigned long u = base == 10 && val < 0 ? -val : val;

	unsigned long p = len;
	do {
		unsigned t =  u % base;
		u /= 10;
		buf[--p] = (t < 10 ? '0' : ('a' - 10)) + t;
	} while (p > 3 && u != 0);
	switch (base) {
		case 2:
			buf[--p] = 'b';
			buf[--p] = '0';
			break;

		case 8:
			buf[--p] = '0';
			break;

		case 10:
			if (val < 0)
				buf[--p] = '-';
			break;

		case 16:
			buf[--p] = 'x';
			buf[--p] = '0';
			break;
	}
	logbuf(buf + p, len - p);
}


int open(const char *pathname, int flags) {
	logmsg("!open(");
	logmsg(pathname);
	logmsg(",");
	lognum(flags, 16);
	logmsg(") = ");
	int fd = sys_open(pathname, flags);
	lognum(fd, 10);
	logmsg(";\n");
	return fd;
}

ssize_t read(int fd, const char * msg, size_t len) {
	logmsg("!read(");
	lognum(fd, 10);
	logmsg(",");
	lognum((long)msg, 16);
	logmsg(",");
	lognum(len, 10);
	logmsg(") = ");
	ssize_t r = sys_read(fd, msg, len);
	lognum(r, 10);
	logmsg(";\n");
	return r;
}

ssize_t write(int fd, const char * msg, size_t len) {
	logmsg("!read(");
	lognum(fd, 10);
	logmsg(",");
	lognum((long)msg, 16);
	logmsg(",");
	lognum(len, 10);
	logmsg(") = ");
	ssize_t r = sys_write(fd, msg, len);
	lognum(r, 10);
	logmsg(";\n");
	return r;
}

int close(int fd) {
	logmsg("!sleep(2);\n");
	sleep(2);

	logmsg("!close(");
	lognum(fd, 10);
	logmsg(") = ");
	int r = sys_close(fd);
	lognum(r, 10);
	logmsg(";\n");
	return r;
}

void exit(int code) {
	logmsg("!exit(");
	lognum(code, 10);
	logmsg(");\n");
	sys_exit(code);
	__builtin_unreachable();
}
