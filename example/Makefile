OPTLEVEL ?= 2

DIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))
LIBNAME := fib
LD_PATH ?= /opt/luci/ld-luci.so

CFLAGS += -O$(OPTLEVEL) -g -fPIC -Wl,-z,relro,-z,now -Wall
LDFLAGS ?= -Wl,--rpath=$(DIR)
ifneq (,$(wildcard $(LD_PATH)))
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

BINSOURCE := $(wildcard main*.c)
BINTARGET := $(BINSOURCE:.c=)
LIBSOURCE := $(wildcard $(LIBNAME)_*.c)
LIBTARGET := $(addprefix lib,$(LIBSOURCE:.c=.so))

all: $(BINTARGET) $(LIBTARGET)

lib%.so: %.c
	$(CC) $(CFLAGS) -Wl,-soname,lib$(LIBNAME).so -shared -o $@ $<

lib%.so: lib%_1.so
	ln -f -s $< $@

%: %.c lib$(LIBNAME).so
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -L$(DIR) -l$(LIBNAME)

