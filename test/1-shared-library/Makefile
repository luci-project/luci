LD_PATH ?= /lib64/ld-linux-x86-64.so.2

CC ?= gcc
CFLAGS ?= -O2 -g -Wall -fPIC
LDFLAGS ?= -Wl,--dynamic-linker=$(LD_PATH)

EXEC ?= run

$(EXEC): main.o lib.so nested.so
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.so: %.o
	$(CC) -shared -o $@ $<
