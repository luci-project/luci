OPTLEVEL ?= 2
CFLAGS += -O$(OPTLEVEL) -g -Wall -Wno-unused-function
LIBDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
LDFLAGS ?= -Wl,-rpath=$(LIBDIR) -ldl

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
LIB ?= extern

$(EXEC): main.c lib$(LIB).so
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -L$(LIBDIR) -l$(LIB)

lib%.so: %.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<
	readelf -D -s "$@" | grep " localized" | grep " IFUNC " > /dev/null
