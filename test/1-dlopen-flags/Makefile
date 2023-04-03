OPTLEVEL ?= 2
CFLAGS += -O$(OPTLEVEL) -g
LIBDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
LDFLAGS ?= -Wl,-rpath=$(LIBDIR) -ldl

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
LIBS ?= one two three four five

$(EXEC): main.c $(addsuffix .so,$(addprefix lib,$(LIBS)))
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -L$(LIBDIR) -l$(word 1,$(LIBS))

lib%.so: %.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<
