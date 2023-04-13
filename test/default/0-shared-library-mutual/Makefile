OPTLEVEL ?= 2
CFLAGS += -O$(OPTLEVEL) -g -Wall
LIBDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
LDFLAGS ?= -Wl,-rpath=$(LIBDIR)
ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run

$(EXEC): main.c libextern.so
	$(CC) $(CFLAGS) -no-pie -fno-pic -o $@ $< $(LDFLAGS) -L$(LIBDIR) -lextern

libextern.so: extern.c libnested.so
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $< $(LDFLAGS) -L$(LIBDIR) -lnested

libnested.so: nested.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<
