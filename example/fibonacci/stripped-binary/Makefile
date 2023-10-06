# current directory
DIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))

# optimization level
OPTLEVEL ?= 2
# C Flags, can be customized as desired...
CFLAGS ?= -O$(OPTLEVEL) -Wall -DDELAY=1

# Linker flags: use full RELRO and set RPATH to look for libraries in current directory and append generic library
LDFLAGS ?= -Wl,-z,relro,-z,now -Wl,--rpath=$(DIR)

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

# build binaries and copy first version in base directory
run-main: $(BINMAIN)
	cp -f $< .

run-measure: $(BINMEASURE)
	cp -f $< .

# Special case for library version 5: link against math library as well
fib_5/run-main fib_5/run-measure: LDFLAGS += -lm

# Build binaries in a subdirectory (with version number)
%/run-main: %.c main.c
	@mkdir -p $*
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	strip --strip-all $@

%/run-measure: %.c measure.c
	@mkdir -p $*
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	strip --strip-all $@

clean::
	rm -f run-main $(BINMAIN) run-measure $(BINMEASURE)
	rmdir fib_*/

.PHONY: all clean
