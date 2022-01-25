OPTLEVEL ?= 2
CFLAGS += -O$(OPTLEVEL) -g -Wall
LIBDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
LDFLAGS ?= -Wl,-rpath=$(LIBDIR)

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
BIN = $(EXEC)-question
LIB = answer
SHARED_LIBS = $(addsuffix .so,$(addprefix lib$(LIB)-,0 1 2))

$(EXEC): $(BIN) $(SHARED_LIBS) $(MAKEFILE_LIST)
	@echo "#!/bin/sh" > $@
	@echo "for lib in $(SHARED_LIBS) ; do ln -f -s \$$lib lib$(LIB).so && echo "Using \$$lib" >&2 ; sleep 4 ; done & " >> $@
	@echo "sleep 2" >> $@
	@echo "./$<" >> $@
	@chmod +x $@

$(BIN): question.c lib$(LIB).so
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -L$(LIBDIR) -l$(LIB)

lib%.so: $(SHARED_LIBS)
	ln -f -s $< $@

lib%-0.so: %.c
	$(CC) $(CFLAGS) -shared -o $@ $<

lib%-1.so: %.c
	$(CC) $(CFLAGS) -DANSWER=23 -shared -o $@ $<

lib%-2.so: %.c
	$(CC) $(CFLAGS) -DANSWER=42 -shared -o $@ $<

FORCE:
