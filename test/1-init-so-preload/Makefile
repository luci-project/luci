OPTLEVEL ?= 2
CFLAGS += -O$(OPTLEVEL) -fPIC -g -Wall
LIBDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
LDFLAGS ?= -Wl,-rpath=$(LIBDIR) -ldl

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
BIN ?= main
SHARED_LIBS := libpreload.so libsnafu.so libbohica.so libfubar.so

all: $(EXEC)

$(EXEC): $(BIN) $(SHARED_LIBS) $(MAKEFILE_LIST)
	@echo "#!/bin/sh" > $@
	@echo "LD_PRELOAD=$(firstword $(SHARED_LIBS)) ./$<" >> $@
	@chmod +x $@

$(BIN): main.o libfubar.so libtarfu.so
	$(CC) $(LDFLAGS) -o $@ $< -L$(LIBDIR) -ltarfu -lfubar

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

libpreload.so: preload.o libsnafu.so libfubar.so
	$(CC) -shared -o $@ $< -L$(LIBDIR) -lsnafu -lfubar

libbohica.so: bohica.o libfubar.so
	$(CC) -shared -o $@ $< -L$(LIBDIR) -lfubar

libfubar.so: fubar.o libsusfu.so
	$(CC) -shared -o $@ $< -L$(LIBDIR) -lsusfu

libsnafu.so: snafu.o libfubar.so
	$(CC) -shared -o $@ $< -L$(LIBDIR) -lfubar

lib%.so: %.o
	$(CC) -shared -o $@ $<
