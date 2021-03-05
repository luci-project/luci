CC = /usr/local/musl/bin/musl-gcc
CFLAGS = --std=gnu11 -Wall -pedantic
LIBSOURCE = answer.c
LIBS = $(addprefix lib,$(LIBSOURCE:.c=.so))
LIBFLAGS = -L. $(addprefix -l,$(LIBSOURCE:.c=))
BINSOURCE = question.c
OBJECTS = $(BINSOURCE:.c=.o)
BINARY = ask

all: $(BINARY) $(BINARY)-self

test: $(BINARY)
	LD_LIBRARY_PATH=. ./$<

$(BINARY)-self: $(OBJECTS) $(LIBSOURCE:.c=.o)
	$(CC) --static -o $@ $^

$(BINARY): $(OBJECTS) $(LIBS)
	$(CC) -o $@ $(OBJECTS) $(LIBFLAGS)

lib%.so: %.c Makefile
	$(CC) $(CFLAGS) -fpic -shared -o $@ $<

%.o: %.c Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm $(LIBFILES) $(OBJECTS)

mrproper: clean
	rm $(BINARY)

.PHONY: all clean mrproper
