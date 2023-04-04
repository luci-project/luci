OPTLEVEL ?= 2
CXXFLAGS += -O$(OPTLEVEL) -g -Wall -fPIC
LIBDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
LDFLAGS ?= -Wl,-rpath=$(LIBDIR)

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
BIN = $(EXEC)-main
LIB = func
SHARED_LIBS = $(addsuffix .so,$(addprefix lib$(LIB)-,0 1 2))

$(EXEC): $(BIN) $(SHARED_LIBS) $(MAKEFILE_LIST)
	@echo "#!/bin/sh" > $@
	@echo "for lib in $(SHARED_LIBS) ; do ln -f -s \$$lib lib$(LIB).so && echo "Using \$$lib" >&2 ; sleep 5 ; done & " >> $@
	@echo "sleep 2" >> $@
	@echo "./$<" >> $@
	@chmod +x $@

$(BIN): main.cpp lib$(LIB).so
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) -L$(LIBDIR) -l$(LIB)

lib%.so: $(SHARED_LIBS)
	ln -f -s $< $@

lib$(LIB)-%.so: $(LIB).cpp Makefile
	$(CXX) $(CXXFLAGS) -DVERSION=$* -shared -o $@ $<

FORCE:
