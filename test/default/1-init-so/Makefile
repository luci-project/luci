OPTLEVEL ?= 2
CFLAGS += -O$(OPTLEVEL) -fPIC -g -Wall
LIBDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
LDFLAGS ?= -Wl,-rpath=$(LIBDIR) -ldl

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run

all: $(EXEC) libbohica.so libfubar.so

$(EXEC): main.o libfubar.so libtarfu.so
	$(CC) $(LDFLAGS) -o $@ $< -L$(LIBDIR) -ltarfu -lfubar

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

libbohica.so: bohica.o libfubar.so
	$(CC) -shared -o $@ $< -L$(LIBDIR) -lfubar

libfubar.so: fubar.o libsusfu.so
	$(CC) -shared -o $@ $< -L$(LIBDIR) -lsusfu

lib%.so: %.o
	$(CC) -shared -o $@ $<
