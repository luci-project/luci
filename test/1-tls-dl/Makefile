CC ?= gcc
OPTLEVEL ?= 2
CFLAGS ?= -O$(OPTLEVEL) -g -Wall -fPIC
LIBDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
LDFLAGS ?= -Wl,-rpath=$(LIBDIR) -L$(LIBDIR) -ldl -pthread

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run

$(EXEC): main.o libfoo.so libbar.so libbaz.so
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lfoo -lbar

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

lib%.so: %.o
	$(CC) -shared -o $@ $<

libbaz.so: baz.o libbaz-helper.so
	$(CC) -shared -o $@ $< $(LDFLAGS) -lbaz-helper
