OPTLEVEL ?= 2
CFLAGS += -O$(OPTLEVEL) -g -Wall -nostdlib -fPIC -fno-exceptions
LIBDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
LDFLAGS ?= -Wl,-rpath=$(LIBDIR)

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
BIN = $(EXEC)-cat
LIB = trace
SHARED_LIBS = $(addsuffix .so,$(addprefix lib$(LIB)-,off on))

$(EXEC): $(BIN) $(SHARED_LIBS) $(MAKEFILE_LIST)
	@echo "#!/bin/sh" > $@
	@echo "for lib in $(SHARED_LIBS) ; do ln -f -s \$$lib lib$(LIB).so && echo "Using \$$lib" >&2 ; sleep 6 ; done & " >> $@
	@echo "sleep .5" >> $@
	@echo "export LD_PRELOAD=$(LIBDIR)lib$(LIB).so" >> $@
	@echo "./$< $(LIBDIR)towe.txt $(LIBDIR)towm.txt $(LIBDIR)towe.txt $(LIBDIR)towm.txt" >> $@
	@chmod +x $@

$(BIN): /bin/cat lib$(LIB).so
	cp $< $@
	if [ -n "$(LD_PATH)" ] ; then $(LIBDIR)/../../../bean/elfo/elfo-setinterp $@ $(LD_PATH) ; fi

lib$(LIB).so: $(SHARED_LIBS)
	ln -f -s $< $@

lib%.so: %.c
	$(CC) $(CFLAGS) -shared -o $@ $<


FORCE:
