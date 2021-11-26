LD_PATH ?= /lib64/ld-linux-x86-64.so.2

CC ?= gcc
CFLAGS ?= -O2 -g -Wall -Wl,--dynamic-linker=$(LD_PATH)

EXEC ?= run
SOURCE = hw.c


$(EXEC): $(SOURCE)
	$(CC) $(CFLAGS) -o $@ $^
