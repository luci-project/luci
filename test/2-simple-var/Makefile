OPTLEVEL ?= 2
CFLAGS += -O$(OPTLEVEL) -g -Wall
LIBDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
LDFLAGS ?= -Wl,-rpath=$(LIBDIR)

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
BIN = $(EXEC)-main
LIB = foo
SHARED_LIBS = $(addsuffix .so,$(addprefix lib$(LIB)-,0 1 2 3 4 5 6 7 8))

$(EXEC): $(BIN) $(SHARED_LIBS) $(MAKEFILE_LIST)
	@echo "#!/bin/sh" > $@
	@echo "for lib in $(SHARED_LIBS) ; do ln -f -s \$$lib lib$(LIB).so && echo "Using \$$lib" >&2 ; sleep 8 ; done & " >> $@
	@echo "sleep 1.5" >> $@
	@echo "./$<" >> $@
	@chmod +x $@

$(BIN): main.c lib$(LIB).so
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -L$(LIBDIR) -l$(LIB)

lib%.so: $(SHARED_LIBS)
	ln -f -s $< $@

lib$(LIB)-%.so: $(LIB).c Makefile
	$(CC) $(CFLAGS) -DVERSION=$* -fPIC --shared -o $@ $<

FORCE:
