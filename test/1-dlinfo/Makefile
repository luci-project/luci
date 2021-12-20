OPTLEVEL ?= 2
CFLAGS += -O$(OPTLEVEL) -g -Wall
LDFLAGS = -Wl,--enable-new-dtags,-rpath=/var/runpath -ldl
ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
BIN = $(EXEC)-main
LIBS ?= foo bar

$(EXEC): $(BIN) $(SHARED_LIBS) $(MAKEFILE_LIST)
	@echo "#!/bin/sh" > $@
	@echo "./$<" >> $@
	@echo "LD_PRELOAD=$$(readlink -f libbar.so) ./$< ./libfoo.so" >> $@
	@chmod +x $@


$(BIN): main.c $(addsuffix .so,$(addprefix lib,$(LIBS)))
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

lib%.so: %.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<
