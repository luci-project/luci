OPTLEVEL ?= 2
CFLAGS += -O$(OPTLEVEL) -g -Wall -fPIC
LIBDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
LDFLAGS ?= -Wl,-rpath=$(LIBDIR) -ldl

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
LIBS ?= foo

$(EXEC): main.o $(addsuffix .so,$(addprefix lib,$(LIBS)))
	$(CC) $(LDFLAGS) -o $@ $< -L$(LIBDIR) $(addprefix -l,$(LIBS))

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

lib%.so: %.o %.map
	$(CC) -shared -Wl,--version-script,$*.map -o $@ $<