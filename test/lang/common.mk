OPTLEVEL ?= 2

DIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))
SETINTERP := $(DIR)/../../../bean/elfo/elfo-setinterp

CFLAGS += -O$(OPTLEVEL) -g -fPIC -Wall

CXXFLAGS += -O$(OPTLEVEL) -g -fPIC -Wall

RUSTC ?= rustc
RUSTFLAGS += -C opt-level=$(OPTLEVEL)

GOBUILD ?= go build
GOBUILDMODE ?= default
GOFLAGS += -a

GNATMAKE ?= gnatmake
GNATDIR = .ada
GNATFLAGS += -D $(GNATDIR) -O$(OPTLEVEL) -g -f

FORTC ?= gfortran
FORTFLAGS += -O$(OPTLEVEL) -g -Wall

FPC ?= fpc
FPCDIR = .pas
FPCFLAGS += -O$(OPTLEVEL) -Tlinux -XD -Cg

LIBNAME = fib
LIBDIR = $(DIR)
LDFLAGS ?= -Wl,--rpath=$(LIBDIR)

SCRIPTVARS += LD_LIBRARY_PATH=$(LIBDIR)

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
	GOLDFLAGS += -ldflags '-I $(LD_PATH)'
endif

EXEC ?= run
BIN = $(EXEC)-main
LIBS = $(addprefix lib$(LIBNAME)_,0 1 2)
SHARED_LIBS = $(addsuffix .so,$(LIBS))

$(EXEC): $(BIN) $(SHARED_LIBS) $(MAKEFILE_LIST)
	@echo "#!/bin/sh" > $@
	@echo "export $(SCRIPTVARS)" > $@
	@echo "for lib in $(SHARED_LIBS) ; do ln -f -s \$$lib lib$(LIBNAME).so && echo "Using \$$lib" >&2 ; sleep 10 ; done & " >> $@
	@echo "sleep 2" >> $@
	@echo "./$<" >> $@
	@chmod +x $@


$(EXEC)-%: %.c lib$(LIBNAME).so
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -L$(LIBDIR) -l$(LIBNAME)

$(EXEC)-%: %.cpp lib$(LIBNAME).so
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) -L$(LIBDIR) -l$(LIBNAME)

$(EXEC)-%: %.rs lib$(LIBNAME).so
	$(RUSTC) $(RUSTFLAGS) -o $@ $< -C link-args='$(LDFLAGS)' -L$(LIBDIR) -l$(LIBNAME)

$(EXEC)-%: %.go lib$(LIBNAME).so
	$(GOBUILD) $(GOFLAGS) $(GOLDFLAGS) -buildmode=$(GOBUILDMODE) -o $@ $<

$(EXEC)-%: %.adb lib$(LIBNAME).so
	$(GNATMAKE)	$(GNATFLAGS) -o $@ $< -largs "$(LDFLAGS)" -L$(LIBDIR)

$(EXEC)-%: %.f90 lib$(LIBNAME).so
	$(FORTC) $(FORTFLAGS) -o $@ $< $(LDFLAGS) -L$(LIBDIR) -l$(LIBNAME)

$(EXEC)-%: %.pas lib$(LIBNAME).so
	$(FPC) $(FPCFLAGS) -FL$(LD_PATH) -o$@ $<


lib%.so: lib%_0.so
	ln -f -s $< $@

lib%.so: %.c
	$(CC) $(CFLAGS) -Wl,-soname,lib$(LIBNAME).so -shared -o $@ $<

lib%.so: %.cpp
	$(CXX) $(CXXFLAGS) -Wl,-soname,lib$(LIBNAME).so -shared -o $@ $<

lib%.so: %.rs
	$(RUSTC) $(RUSTFLAGS) --crate-type=dylib -C link-args='-Wl,-soname,lib$(LIBNAME).so' -o $@ $<

lib%.so: %.go
	$(GOBUILD) $(GOFLAGS) -compiler gccgo -gccgoflags '-O$(OPTLEVEL) $(LDFLAGS) -Wl,-soname,lib$(LIBNAME).so' -buildmode=c-shared -o $@ $<

$(GNATDIR)/%.o : %.ads %.adb
	@mkdir -p $(GNATDIR)
	$(GNATMAKE)	$(GNATFLAGS) -shared -c $*

lib%.so: $(GNATDIR)/%.o
	$(CC) -shared -Wl,-soname,lib$(LIBNAME).so -o $@ $< -lgnat

lib%.so: %.f90
	$(FORTC) $(FORTFLAGS) -shared -o$@ $<

lib%.so: %.pas
	@mkdir -p $(FPCDIR)
	$(FPC) $(FPCFLAGS) -CD -o$(FPCDIR)/lib$(LIBNAME).so $<
	@mv -f $(FPCDIR)/lib$(LIBNAME).so $@


.SECONDARY: lib$(LIBNAME).so

FORCE:
