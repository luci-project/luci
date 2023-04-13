OPTLEVEL ?= 2
CXXFLAGS += -O$(OPTLEVEL) -g -Wall -fPIC
LIBDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
LDFLAGS ?= -Wl,-rpath=$(LIBDIR)

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
BIN = $(EXEC)-main
LIBFOO = foo
LIBBAR = bar
LIBBAZ = baz
SHARED_LIBS = $(addsuffix .so,$(addprefix lib$(LIBFOO)-,0 1 2) lib$(LIBBAR) $(addprefix lib$(LIBBAZ)-,0 1 2))

$(EXEC): $(BIN) $(SHARED_LIBS) $(MAKEFILE_LIST)
	@echo "#!/bin/bash" > $@
	@echo "for vers in foo-0 foo-1 baz-1 foo-2 baz-2 ; do ln -f -s lib\$${vers}.so lib\$${vers%-*}.so && echo "Using lib\$${vers}.so" >&2 ; sleep 4 ; done & " >> $@
	@echo "sleep 2" >> $@
	@echo "./$<" >> $@
	@chmod +x $@

$(BIN): main.cpp lib$(LIBFOO).so
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) -L$(LIBDIR) -l$(LIBFOO)

lib$(LIBFOO).so: $(addsuffix .so,$(addprefix lib$(LIBFOO)-,0 1 2))
	ln -f -s $< $@

lib$(LIBFOO)-%.so: $(LIBFOO).cpp lib$(LIBBAR).so Makefile
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -DVERSION=$* -shared -o $@ $< -L$(LIBDIR) -l$(LIBBAR)

lib$(LIBBAR).so: $(LIBBAR).cpp lib$(LIBBAZ).so Makefile
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $< -L$(LIBDIR) -l$(LIBBAZ)

lib$(LIBBAZ).so: $(addsuffix .so,$(addprefix lib$(LIBBAZ)-,0 1 2))
	ln -f -s $< $@

lib$(LIBBAZ)-%.so: $(LIBBAZ).cpp Makefile
	$(CXX) $(CXXFLAGS) -DVERSION=$* -shared -o $@ $<

FORCE:
