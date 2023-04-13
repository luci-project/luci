OPTLEVEL ?= 2
CFLAGS += -O$(OPTLEVEL) -g -Wall
LIBDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
LDFLAGS ?= -Wl,-rpath=$(LIBDIR)

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
LIB ?= extern

$(EXEC): main.c lib$(LIB).so
	$(CC) $(CFLAGS) -o $@ $< -L$(LIBDIR) -l$(LIB) $(LDFLAGS)

lib%.so: %.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<
