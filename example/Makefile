# current directory
DIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))

# Name of the shared library
LIBNAME := fib

# optimization level
OPTLEVEL ?= 2
# C Flags, can be customized as desired...
CFLAGS ?= -O$(OPTLEVEL) -gdwarf-4 -Wall -DDELAY=1
# ... only -fPIC (for position independent code) is required for shared library
CFLAGS += -fPIC

# Linker flags: use full RELRO
LDFLAGS ?= -Wl,-z,relro,-z,now
# For library, use LDFLAGS
# For library, use LDFLAGS and set generic SONAME
LIBLDFLAGS := $(LDFLAGS) -Wl,-soname,lib$(LIBNAME).so
# For binary, set RPATH to look for libraries in current directory and append generic library
BINLDFLAGS := $(LDFLAGS) -Wl,--rpath=$(DIR) -L$(DIR) -l$(LIBNAME)

# Default Luci interpreter path...
LD_PATH ?= /opt/luci/ld-luci.so
# ... and if LD_PATH exists...
ifneq (,$(wildcard $(LD_PATH)))
	# ... set it as interpreter for binary
	BINLDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

# Libraries
LIBSOURCE := $(sort $(wildcard $(LIBNAME)_*.c))
LIBTARGET := $(patsubst %.c,%/lib$(LIBNAME).so,$(LIBSOURCE))
# Binaries
BINSOURCE := $(filter-out $(LIBSOURCE),$(wildcard *.c))
BINTARGET := $(patsubst %.c,run-%,$(BINSOURCE))

# build everything
all: bin lib

# build binaries
bin: $(BINTARGET)

# Build all libraries and a symbolic link to first version in base directory
lib: $(LIBTARGET)
	ln -f -s $< lib$(LIBNAME).so

# Special case for library version 5: link against math library as well
fib_5/libfib.so: LIBLDFLAGS += -lm

# Build relocatable object file
%.o: %.c $(MAKEFILE_LIST)
	$(CC) $(CFLAGS) -c -o $@ $<

# Build shared library in a subdirectory (with version number)
%/lib$(LIBNAME).so: %.o
	@mkdir -p $*
	$(CC) -shared -o $@ $< $(LIBLDFLAGS) 

# Build ELF file.
run-%: %.o lib
	$(CC) -o $@ $< $(BINLDFLAGS)

clean::
	rm -f $(BINTARGET) $(LIBTARGET) lib$(LIBNAME).so
	rmdir fib_*/

.PHONY: all bin lib clean
