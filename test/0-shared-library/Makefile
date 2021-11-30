LD_PATH ?= /lib64/ld-linux-x86-64.so.2

CC ?= gcc
CFLAGS ?= -O2 -g -Wall -fPIC
LDFLAGS ?= -Wl,--dynamic-linker=$(LD_PATH)

EXEC ?= run
LIBS ?= extern nested

$(EXEC): main.o $(addsuffix .so,$(addprefix lib,$(LIBS)))
	$(CC) $(LDFLAGS) -o $@ $< -L. $(addprefix -l,$(LIBS))

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

lib%.so: %.o
	$(CC) -shared -o $@ $<
