# current directory
DIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))

# Name of the shared library
LIBNAME := fib

# optimization level
OPTLEVEL ?= 2
# C Flags, can be customized as desired...
CFLAGS ?= -O$(OPTLEVEL) -g -Wall -DDELAY=1

FIBSOURCE := $(sort $(wildcard fib_*.c))
OBJECTS := main.o measure.o $(patsubst %.c,%/fib.o,$(FIBSOURCE)) $(patsubst %.c,%/lib$(LIBNAME).a,$(FIBSOURCE))

# compile everything
all: $(OBJECTS)

# Build relocatable object file
%.o: %.c $(MAKEFILE_LIST)
	$(CC) $(CFLAGS) -c -o $@ $<

# Build object files in a subdirectory (with version number)
%/$(LIBNAME).o: %.c
	@mkdir -p $*
	$(CC) $(CFLAGS) -c -o $@ $<

%/lib$(LIBNAME).a: %/$(LIBNAME).o
	@mkdir -p $*
	ar rcs $@ $*/$(LIBNAME).o


clean::
	rm -f $(BINTARGET) $(LIBTARGET) lib$(LIBNAME).so
	rmdir fib_*/

.PHONY: all clean
