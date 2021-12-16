OPTLEVEL ?= 2
CXXFLAGS ?= -O$(OPTLEVEL) -g -Wall -fPIC
LIBDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
LDFLAGS ?= -Wl,-rpath=$(LIBDIR) -ldl

ifdef LD_PATH
	LDFLAGS += -Wl,--dynamic-linker=$(LD_PATH)
endif

EXEC ?= run
LIBS ?= extern

$(EXEC): main.o $(addsuffix .so,$(addprefix lib,$(LIBS)))
	$(CXX) $(LDFLAGS) -o $@ $< -L$(LIBDIR) $(addprefix -l,$(LIBS))

%.o: %.c
	$(CXX) $(CXXFLAGS) -c -o $@ $<

lib%.so: %.o
	$(CXX) -shared -o $@ $<
