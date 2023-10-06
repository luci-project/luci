# current directory
DIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))

# Name of the shared library
LIBNAME := fib

# optimization level
OPTLEVEL ?= 2
# C Flags, can be customized as desired...
CFLAGS ?= -O$(OPTLEVEL) -gdwarf-4 -Wall -DDELAY=1

# Linker flags: use full RELRO
LDFLAGS ?= -Wl,-z,relro,-z,now -l$(LIBNAME)

# Default Luci interpreter path...
LD_PATH ?= /opt/luci/ld-luci.so
# ... and if LD_PATH exists...
ifneq (,$(wildcard $(LD_PATH)))
	# ... set it as interpreter for binary
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif


FIBSOURCE := $(sort $(wildcard fib_*.c))
BINMAIN := $(patsubst %.c,%/run-main,$(FIBSOURCE))
BINMEASURE := $(patsubst %.c,%/run-measure,$(FIBSOURCE))


# build everything
all: run-main run-measure

# build binaries and a symbolic link to first version in base directory
run-main: $(BINMAIN)
	ln -f -s $<

run-measure: $(BINMEASURE)
	ln -f -s $<

# Build relocatable object file
%.o: %.c $(MAKEFILE_LIST)
	$(CC) $(CFLAGS) -c -o $@ $<

# Build shared library in a subdirectory (with version number)
%/lib$(LIBNAME).a: %.o
	@mkdir -p $*
	ar rcs $@ $<

# Special case for library version 5: link against math library as well
fib_5/run-main fib_5/run-measure: LDFLAGS += -lm

# Build binaries in a subdirectory (with version number)
%/run-main: main.o %/lib$(LIBNAME).a
	@mkdir -p $*
	$(CC) $(CFLAGS) -o $@ $< -L$* $(LDFLAGS)

%/run-measure: measure.o %/lib$(LIBNAME).a
	@mkdir -p $*
	$(CC) $(CFLAGS) -o $@ $< -L$* $(LDFLAGS)

clean::
	rm -f $(BINTARGET) $(LIBTARGET) lib$(LIBNAME).so
	rmdir fib_*/

.PHONY: all bin lib clean
