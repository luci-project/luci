RTLD ?= /lib64/ld-linux-x86-64.so.2

CC ?= gcc
CFLAGS ?= -O2 -g -Wall -Wl,--dynamic-linker=$(RTLD)

BINARY ?= run
SOURCE = hw.c


$(BINARY): $(SOURCE)
	$(CC) $(CFLAGS) -o $@ $<


